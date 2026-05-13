/*
 * Component-side bindings.
 *
 *   registerComponent(reg, name, className, fieldNames[], fieldTags[])
 *      Hashes `name` to an entt::id_type, stores the schema in the
 *      registry's ctx-state, and pre-creates the pool via
 *      registry.storage<ScriptComponent>(id).
 *
 *   emplace / get / try_get / has / all_of / any_of / remove / patch
 *      All operate on the (registry, entity, component-name) triple. The
 *      typed pool is recovered via storage<ScriptComponent>(schema.id).
 */

#include "BindingHelpers.hpp"
#include "ScriptComponent.hpp"

#include <entt.hpp>

namespace mtype_entt
{
    namespace
    {
        constexpr const char* kEx = "EnttError";

        inline entt::entity asEntity(int64_t v) {
            return static_cast<entt::entity>(static_cast<std::uint32_t>(v));
        }

        /* Bounded array readers that raise on type tag mismatch. */
        bool readStringArray(MTypeContext* ctx, const MTypeValue* arr,
                              std::vector<std::string>& out, const char* op)
        {
            if (g_host->getTag(arr) != MT_TAG_ARRAY) {
                g_host->raiseError(ctx, kEx,
                    (std::string(op) + ": expected array of strings").c_str());
                return false;
            }
            const std::size_t n = g_host->arrayLen(arr);
            out.clear();
            out.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                MTypeValue* v = g_host->arrayGet(ctx, arr, i);
                std::size_t len = 0;
                const char* s = g_host->getString(v, &len);
                if (!s) {
                    g_host->raiseError(ctx, kEx,
                        (std::string(op) + ": non-string element").c_str());
                    return false;
                }
                out.emplace_back(s, len);
            }
            return true;
        }

        bool readIntArray(MTypeContext* ctx, const MTypeValue* arr,
                           std::vector<int64_t>& out, const char* op)
        {
            if (g_host->getTag(arr) != MT_TAG_ARRAY) {
                g_host->raiseError(ctx, kEx,
                    (std::string(op) + ": expected array of ints").c_str());
                return false;
            }
            const std::size_t n = g_host->arrayLen(arr);
            out.clear();
            out.reserve(n);
            for (std::size_t i = 0; i < n; ++i) {
                MTypeValue* v = g_host->arrayGet(ctx, arr, i);
                out.push_back(g_host->getInt(v));
            }
            return true;
        }

        MTypeValue* nRegisterComponent(void*, MTypeContext* ctx,
                                         const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_register_component";
            if (!detail::requireArgs(ctx, argc, 5, op, kEx))
                return g_host->makeVoid(ctx);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);

            std::size_t nameLen = 0, classLen = 0;
            const char* name      = g_host->getString(args[1], &nameLen);
            const char* className = g_host->getString(args[2], &classLen);
            std::vector<std::string> fieldNames;
            std::vector<int64_t>     fieldTagsRaw;
            if (!readStringArray(ctx, args[3], fieldNames,    op)) return g_host->makeVoid(ctx);
            if (!readIntArray  (ctx, args[4], fieldTagsRaw, op)) return g_host->makeVoid(ctx);
            if (fieldNames.size() != fieldTagsRaw.size()) {
                g_host->raiseError(ctx, kEx,
                    "register_component: field-name and field-tag arrays must have equal length");
                return g_host->makeVoid(ctx);
            }

            ComponentSchema sch;
            sch.name      = std::string(name, nameLen);
            sch.className = std::string(className, classLen);
            sch.fieldNames = std::move(fieldNames);
            sch.fieldTags.reserve(fieldTagsRaw.size());
            for (auto t : fieldTagsRaw) sch.fieldTags.push_back(static_cast<FieldTag>(t));
            sch.id = entt::hashed_string{sch.name.c_str()}.value();

            auto& st = stateOf(*reg);
            st.nameById[sch.id] = sch.name;

            // Pre-create the typed pool so storage<ScriptComponent>(id)
            // exists before view/runtime_view calls reach for it.
            (void)reg->storage<ScriptComponent>(sch.id);

            st.schemasByName[sch.name] = std::move(sch);
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nEmplace(void*, MTypeContext* ctx,
                              const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_emplace";
            if (!detail::requireArgs(ctx, argc, 4, op, kEx))
                return g_host->makeVoid(ctx);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeVoid(ctx);

            ScriptComponent comp = buildFromObject(*sch, args[3], ctx);
            auto& pool = reg->storage<ScriptComponent>(sch->id);
            // basic_storage has no `emplace_or_replace` — that helper lives
            // on registry, but only for compile-time-known types. Branch on
            // contains() instead.
            if (pool.contains(e)) {
                pool.patch(e, [&](ScriptComponent& dst) { dst = std::move(comp); });
            } else {
                pool.emplace(e, std::move(comp));
            }
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nEmplaceTag(void*, MTypeContext* ctx,
                                  const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_emplace_tag";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeVoid(ctx);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeVoid(ctx);

            auto& pool = reg->storage<ScriptComponent>(sch->id);
            if (!pool.contains(e)) pool.emplace(e, ScriptComponent{});
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nGet(void*, MTypeContext* ctx,
                          const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_get";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeNull(ctx);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeNull(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeNull(ctx);

            auto& pool = reg->storage<ScriptComponent>(sch->id);
            if (!pool.contains(e)) {
                std::string m = std::string(op) + ": entity has no component '" + sch->name + "'";
                g_host->raiseError(ctx, kEx, m.c_str());
                return g_host->makeNull(ctx);
            }
            return writeIntoObject(*sch, pool.get(e), ctx);
        }

        MTypeValue* nTryGet(void*, MTypeContext* ctx,
                              const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_try_get";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeNull(ctx);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeNull(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeNull(ctx);

            auto& pool = reg->storage<ScriptComponent>(sch->id);
            if (!pool.contains(e)) return g_host->makeNull(ctx);
            return writeIntoObject(*sch, pool.get(e), ctx);
        }

        MTypeValue* nHas(void*, MTypeContext* ctx,
                          const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_has";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeBool(ctx, 0);

            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeBool(ctx, 0);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeBool(ctx, 0);

            auto& pool = reg->storage<ScriptComponent>(sch->id);
            return g_host->makeBool(ctx, pool.contains(e) ? 1 : 0);
        }

        /* Shared helper for all_of / any_of. names = array of component names. */
        int countPresent(entt::registry& reg, entt::entity e,
                          const MTypeValue* names, MTypeContext* ctx, const char* op,
                          int* outTotal)
        {
            int present = 0;
            const std::size_t n = g_host->arrayLen(names);
            *outTotal = static_cast<int>(n);
            for (std::size_t i = 0; i < n; ++i) {
                MTypeValue* v = g_host->arrayGet(ctx, names, i);
                std::size_t len = 0;
                const char* s = g_host->getString(v, &len);
                const ComponentSchema* sch = resolveSchema(reg, std::string(s, len), ctx, op);
                if (!sch) return present;
                if (reg.storage<ScriptComponent>(sch->id).contains(e)) ++present;
            }
            return present;
        }

        MTypeValue* nAllOf(void*, MTypeContext* ctx,
                            const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_all_of";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeBool(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeBool(ctx, 0);
            const entt::entity e = asEntity(detail::getI(args[1]));
            int total = 0;
            const int p = countPresent(*reg, e, args[2], ctx, op, &total);
            return g_host->makeBool(ctx, p == total ? 1 : 0);
        }

        MTypeValue* nAnyOf(void*, MTypeContext* ctx,
                            const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_any_of";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeBool(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeBool(ctx, 0);
            const entt::entity e = asEntity(detail::getI(args[1]));
            int total = 0;
            const int p = countPresent(*reg, e, args[2], ctx, op, &total);
            return g_host->makeBool(ctx, p > 0 ? 1 : 0);
        }

        MTypeValue* nRemove(void*, MTypeContext* ctx,
                              const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_remove";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeVoid(ctx);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeVoid(ctx);
            reg->storage<ScriptComponent>(sch->id).remove(e);
            return g_host->makeVoid(ctx);
        }

        /* patch = read fields from the supplied object, overwrite the stored
         * component, and fire the on_update sigh. */
        MTypeValue* nPatch(void*, MTypeContext* ctx,
                             const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_patch";
            if (!detail::requireArgs(ctx, argc, 4, op, kEx))
                return g_host->makeVoid(ctx);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]), ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[2], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeVoid(ctx);

            auto& pool = reg->storage<ScriptComponent>(sch->id);
            if (!pool.contains(e)) {
                std::string m = std::string(op) + ": entity has no component '" + sch->name + "'";
                g_host->raiseError(ctx, kEx, m.c_str());
                return g_host->makeVoid(ctx);
            }
            ScriptComponent next = buildFromObject(*sch, args[3], ctx);
            pool.patch(e, [&](ScriptComponent& dst) { dst = std::move(next); });
            return g_host->makeVoid(ctx);
        }
    }

    void registerComponentNatives(MTypeContext* ctx)
    {
        detail::Registrar r{ctx, "__native__entt_"};
        r("register_component", &nRegisterComponent)
         ("emplace",             &nEmplace)
         ("emplace_tag",         &nEmplaceTag)
         ("get",                 &nGet)
         ("try_get",             &nTryGet)
         ("has",                 &nHas)
         ("all_of",              &nAllOf)
         ("any_of",              &nAnyOf)
         ("remove",              &nRemove)
         ("patch",               &nPatch);
    }
}
