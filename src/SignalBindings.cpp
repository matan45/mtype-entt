/*
 * Signal bridge: registry.on_construct/update/destroy -> mType function call.
 *
 * EnTT's sigh signature for component events is `void(registry&, entity)`.
 * Inside the listener we recover the current MTypeContext* via the ScopedCtx
 * thread-local stack and call back into mType with [registryHandle, entity].
 *
 * SignalCallback instances are owned by PerRegistryState::signalCallbacks
 * (unique_ptr so the address stays stable across vector reallocations and
 * the const char*'s the sigh-bound member fn dereferences keep working).
 */

#include "BindingHelpers.hpp"
#include "ScriptComponent.hpp"

#include <entt.hpp>

#include <list>

namespace mtype_entt
{
    /* Definition for the forward-declared struct in ScriptComponent.hpp. */
    struct SignalListener
    {
        int64_t       registryHandle = 0;
        std::string   componentName;
        std::string   functionName;

        void fire(entt::registry&, entt::entity e)
        {
            MTypeContext* ctx = detail::currentCtx();
            if (!ctx) return;  // shouldn't happen — signals fire under a ScopedCtx
            MTypeValue* a0 = g_host->makeInt(ctx, registryHandle);
            MTypeValue* a1 = g_host->makeInt(ctx, static_cast<int64_t>(
                static_cast<std::uint32_t>(entt::to_integral(e))));
            const MTypeValue* argv[2] = { a0, a1 };
            g_host->callFunction(ctx, functionName.c_str(), argv, 2);
        }
    };

    namespace
    {
        constexpr const char* kEx = "EnttError";

        enum class SignalKind { Construct, Update, Destroy };

        /* Per-registry holder for SignalListener instances. std::list keeps
         * element addresses stable across push_back, so the EnTT sink
         * holds a permanently-valid pointer for the registry's lifetime.
         * Move-only — entt::ctx never copies a stored value. */
        struct RegSideTable {
            std::list<SignalListener> listeners;
            RegSideTable() = default;
            RegSideTable(const RegSideTable&)            = delete;
            RegSideTable& operator=(const RegSideTable&) = delete;
            RegSideTable(RegSideTable&&) noexcept            = default;
            RegSideTable& operator=(RegSideTable&&) noexcept = default;
        };

        RegSideTable& sideTableOf(entt::registry& reg)
        {
            auto& cx = reg.ctx();
            if (auto* p = cx.find<RegSideTable>()) return *p;
            return cx.emplace<RegSideTable>();
        }

        MTypeValue* connectSignal(SignalKind kind, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc,
                                    const char* op)
        {
            detail::ScopedCtx _cx(ctx);
            if (!detail::requireArgs(ctx, argc, 3, op, kEx))
                return g_host->makeVoid(ctx);
            const int64_t regHandle = detail::getI(args[0]);
            auto* reg = detail::findOrRaise(g_registries, regHandle, ctx, op, kEx);
            if (!reg) return g_host->makeVoid(ctx);

            std::size_t nameLen = 0;
            const char* name = g_host->getString(args[1], &nameLen);
            const ComponentSchema* sch = resolveSchema(*reg, std::string(name, nameLen), ctx, op);
            if (!sch) return g_host->makeVoid(ctx);

            std::size_t fnLen = 0;
            const char* fnName = g_host->getString(args[2], &fnLen);
            if (!fnName || fnLen == 0) {
                g_host->raiseError(ctx, kEx,
                    (std::string(op) + ": function name must be a non-empty string").c_str());
                return g_host->makeVoid(ctx);
            }

            auto& table = sideTableOf(*reg);
            SignalListener& raw = table.listeners.emplace_back();
            raw.registryHandle = regHandle;
            raw.componentName  = sch->name;
            raw.functionName.assign(fnName, fnLen);

            switch (kind) {
                case SignalKind::Construct:
                    reg->on_construct<ScriptComponent>(sch->id)
                       .connect<&SignalListener::fire>(raw);
                    break;
                case SignalKind::Update:
                    reg->on_update<ScriptComponent>(sch->id)
                       .connect<&SignalListener::fire>(raw);
                    break;
                case SignalKind::Destroy:
                    reg->on_destroy<ScriptComponent>(sch->id)
                       .connect<&SignalListener::fire>(raw);
                    break;
            }
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nOnConstruct(void*, MTypeContext* ctx,
                                   const MTypeValue* const* args, int argc)
        { return connectSignal(SignalKind::Construct, ctx, args, argc, "__native__entt_on_construct"); }
        MTypeValue* nOnUpdate(void*, MTypeContext* ctx,
                                const MTypeValue* const* args, int argc)
        { return connectSignal(SignalKind::Update, ctx, args, argc, "__native__entt_on_update"); }
        MTypeValue* nOnDestroy(void*, MTypeContext* ctx,
                                 const MTypeValue* const* args, int argc)
        { return connectSignal(SignalKind::Destroy, ctx, args, argc, "__native__entt_on_destroy"); }
    }

    void registerSignalNatives(MTypeContext* ctx)
    {
        detail::Registrar r{ctx, "__native__entt_"};
        r("on_construct", &nOnConstruct)
         ("on_update",    &nOnUpdate)
         ("on_destroy",   &nOnDestroy);
    }
}
