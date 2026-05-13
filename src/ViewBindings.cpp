/*
 * Lazy queries via entt::runtime_view.
 *
 * EnttView wraps a runtime_view together with the int handle of the owning
 * registry, so every view op can re-validate that the registry hasn't been
 * destroyed under it. runtime_view borrows storage references; if the
 * registry goes away, the references dangle, so we refuse to iterate.
 */

#include "BindingHelpers.hpp"
#include "ScriptComponent.hpp"

#include <entt.hpp>

#include <vector>

namespace mtype_entt
{
    /* PluginGlobals.hpp forward-declares this; the definition lives here. */
    struct EnttView
    {
        int64_t                  registryHandle = 0;
        entt::runtime_view       rv;
        // For next() iteration we materialize a vector once, then step
        // through it. runtime_view::begin/end produce input iterators only;
        // a snapshot is simpler and stable across script-side calls.
        std::vector<entt::entity> snapshot;
        std::size_t              cursor = 0;
        bool                     iteratorReady = false;
    };

    namespace
    {
        constexpr const char* kEx = "EnttError";

        inline int64_t fromEntity(entt::entity e) {
            return static_cast<int64_t>(static_cast<std::uint32_t>(entt::to_integral(e)));
        }

        /* Resolve component names against the registry's schema map and
         * push their storages into the runtime_view. Returns false on
         * unknown name (and raises). */
        bool addStorages(entt::registry& reg, MTypeContext* ctx,
                          const MTypeValue* names,
                          entt::runtime_view& rv, bool excluding,
                          const char* op)
        {
            if (g_host->getTag(names) != MT_TAG_ARRAY) {
                g_host->raiseError(ctx, kEx,
                    (std::string(op) + ": expected array of component names").c_str());
                return false;
            }
            const std::size_t n = g_host->arrayLen(names);
            for (std::size_t i = 0; i < n; ++i) {
                MTypeValue* v = g_host->arrayGet(ctx, names, i);
                std::size_t len = 0;
                const char* s = g_host->getString(v, &len);
                const ComponentSchema* sch = resolveSchema(reg, std::string(s, len), ctx, op);
                if (!sch) return false;
                // type-erased pool reference; runtime_view::iterate takes
                // a base storage&.
                auto* base = reg.storage(sch->id);
                if (!base) {
                    g_host->raiseError(ctx, kEx,
                        (std::string(op) + ": component pool missing '" + sch->name + "'").c_str());
                    return false;
                }
                if (excluding) rv.exclude(*base);
                else            rv.iterate(*base);
            }
            return true;
        }

        MTypeValue* nView(void*, MTypeContext* ctx,
                            const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx))
                return g_host->makeInt(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeInt(ctx, 0);

            auto* view = new EnttView();
            view->registryHandle = detail::getI(args[0]);
            if (!addStorages(*reg, ctx, args[1], view->rv, false, op)) {
                delete view;
                return g_host->makeInt(ctx, 0);
            }
            return g_host->makeInt(ctx, g_views.insert(view));
        }

        MTypeValue* nViewExcluding(void*, MTypeContext* ctx,
                                     const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view_excluding";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeInt(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeInt(ctx, 0);

            auto* view = new EnttView();
            view->registryHandle = detail::getI(args[0]);
            if (!addStorages(*reg, ctx, args[1], view->rv, false, op) ||
                !addStorages(*reg, ctx, args[2], view->rv, true,  op))
            {
                delete view;
                return g_host->makeInt(ctx, 0);
            }
            return g_host->makeInt(ctx, g_views.insert(view));
        }

        bool ensureOwnerAlive(EnttView& v, MTypeContext* ctx, const char* op)
        {
            if (!g_registries.find(v.registryHandle)) {
                g_host->raiseError(ctx, kEx,
                    (std::string(op) + ": owning registry was destroyed").c_str());
                return false;
            }
            return true;
        }

        MTypeValue* nViewEntities(void*, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view_entities";
            if (!detail::requireArgs(ctx, argc, 1, op, kEx))
                return g_host->makeArray(ctx, MT_TAG_INT, 0);
            auto* view = detail::findOrRaise(g_views, detail::getI(args[0]), ctx, op, kEx);
            if (!view) return g_host->makeArray(ctx, MT_TAG_INT, 0);
            if (!ensureOwnerAlive(*view, ctx, op))
                return g_host->makeArray(ctx, MT_TAG_INT, 0);

            std::vector<entt::entity> es;
            for (auto e : view->rv) es.push_back(e);

            MTypeValue* out = g_host->makeArray(ctx, MT_TAG_INT, es.size());
            for (std::size_t i = 0; i < es.size(); ++i) {
                g_host->arraySet(out, i, g_host->makeInt(ctx, fromEntity(es[i])));
            }
            return out;
        }

        /* next(): returns the next entity id, or 0 when exhausted. The
         * first call materializes a snapshot (so the iteration is stable
         * even if scripts emplace/remove during the loop). */
        MTypeValue* nViewNext(void*, MTypeContext* ctx,
                                const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view_next";
            if (!detail::requireArgs(ctx, argc, 1, op, kEx))
                return g_host->makeInt(ctx, 0);
            auto* view = detail::findOrRaise(g_views, detail::getI(args[0]), ctx, op, kEx);
            if (!view) return g_host->makeInt(ctx, 0);
            if (!ensureOwnerAlive(*view, ctx, op))
                return g_host->makeInt(ctx, 0);

            if (!view->iteratorReady) {
                view->snapshot.clear();
                for (auto e : view->rv) view->snapshot.push_back(e);
                view->cursor = 0;
                view->iteratorReady = true;
            }
            if (view->cursor >= view->snapshot.size())
                return g_host->makeInt(ctx, 0);
            return g_host->makeInt(ctx, fromEntity(view->snapshot[view->cursor++]));
        }

        MTypeValue* nViewReset(void*, MTypeContext* ctx,
                                 const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view_reset";
            if (!detail::requireArgs(ctx, argc, 1, op, kEx))
                return g_host->makeVoid(ctx);
            auto* view = detail::findOrRaise(g_views, detail::getI(args[0]), ctx, op, kEx);
            if (!view) return g_host->makeVoid(ctx);
            view->cursor = 0;
            view->iteratorReady = false;
            view->snapshot.clear();
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nViewDestroy(void*, MTypeContext* ctx,
                                   const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_view_destroy";
            if (!detail::requireArgs(ctx, argc, 1, op, kEx))
                return g_host->makeVoid(ctx);
            delete g_views.erase(detail::getI(args[0]));
            return g_host->makeVoid(ctx);
        }
    }

    void registerViewNatives(MTypeContext* ctx)
    {
        detail::Registrar r{ctx, "__native__entt_"};
        r("view",           &nView)
         ("view_excluding", &nViewExcluding)
         ("view_entities",  &nViewEntities)
         ("view_next",      &nViewNext)
         ("view_reset",     &nViewReset)
         ("view_destroy",   &nViewDestroy);
    }
}
