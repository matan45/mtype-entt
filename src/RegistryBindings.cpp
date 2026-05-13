/*
 * Registry lifecycle and entity ops.
 *
 *   entt registry handle <-> int64 id (via g_registries).
 *   entt::entity <-> int64 (raw to_integral conversion).
 *
 * Note: entt::null is the sentinel for "no entity". It is *not* zero;
 * however, scripts only see entity ids that came from create(), so we
 * preserve to_integral(entt::entity) verbatim and let valid() decide.
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
        inline int64_t fromEntity(entt::entity e) {
            return static_cast<int64_t>(static_cast<std::uint32_t>(entt::to_integral(e)));
        }

        MTypeValue* nRegistryCreate(void*, MTypeContext* ctx,
                                     const MTypeValue* const*, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 0, "__native__entt_registry_create", kEx))
                return g_host->makeInt(ctx, 0);
            auto* reg = new entt::registry();
            // Force ctx state creation up front so signal-callback storage exists.
            stateOf(*reg);
            return g_host->makeInt(ctx, g_registries.insert(reg));
        }

        MTypeValue* nRegistryDestroy(void*, MTypeContext* ctx,
                                      const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 1, "__native__entt_registry_destroy", kEx))
                return g_host->makeVoid(ctx);
            auto* reg = g_registries.erase(detail::getI(args[0]));
            delete reg;
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nRegistryClear(void*, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 1, "__native__entt_registry_clear", kEx))
                return g_host->makeVoid(ctx);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]),
                                             ctx, "__native__entt_registry_clear", kEx);
            if (!reg) return g_host->makeVoid(ctx);
            reg->clear();
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nRegistrySize(void*, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 1, "__native__entt_registry_size", kEx))
                return g_host->makeInt(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]),
                                             ctx, "__native__entt_registry_size", kEx);
            if (!reg) return g_host->makeInt(ctx, 0);
            // free_list() on the entity-storage returns the alive count
            // (alive entities occupy [0, free_list); destroyed slots sit
            // beyond and form an intrusive free chain).
            const auto alive = static_cast<int64_t>(reg->storage<entt::entity>().free_list());
            return g_host->makeInt(ctx, alive);
        }

        MTypeValue* nCreate(void*, MTypeContext* ctx,
                             const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 1, "__native__entt_create", kEx))
                return g_host->makeInt(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]),
                                             ctx, "__native__entt_create", kEx);
            if (!reg) return g_host->makeInt(ctx, 0);
            return g_host->makeInt(ctx, fromEntity(reg->create()));
        }

        MTypeValue* nDestroyEntity(void*, MTypeContext* ctx,
                                     const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 2, "__native__entt_destroy_entity", kEx))
                return g_host->makeVoid(ctx);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]),
                                             ctx, "__native__entt_destroy_entity", kEx);
            if (!reg) return g_host->makeVoid(ctx);
            const entt::entity e = asEntity(detail::getI(args[1]));
            if (reg->valid(e)) reg->destroy(e);
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nValid(void*, MTypeContext* ctx,
                            const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 2, "__native__entt_valid", kEx))
                return g_host->makeBool(ctx, 0);
            auto* reg = detail::findOrRaise(g_registries, detail::getI(args[0]),
                                             ctx, "__native__entt_valid", kEx);
            if (!reg) return g_host->makeBool(ctx, 0);
            const entt::entity e = asEntity(detail::getI(args[1]));
            return g_host->makeBool(ctx, reg->valid(e) ? 1 : 0);
        }
    }

    void registerRegistryNatives(MTypeContext* ctx)
    {
        detail::Registrar r{ctx, "__native__entt_"};
        r("registry_create",  &nRegistryCreate)
         ("registry_destroy", &nRegistryDestroy)
         ("registry_clear",   &nRegistryClear)
         ("registry_size",    &nRegistrySize)
         ("create",           &nCreate)
         ("destroy_entity",   &nDestroyEntity)
         ("valid",            &nValid);
    }
}
