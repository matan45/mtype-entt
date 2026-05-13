/*
 * Plugin entry point + cross-TU globals.
 *
 * `mtype_plugin_register` is called exactly once when the .mt script
 * runs `__plugin_load("mt/mtype_entt.dll")`. It validates the ABI
 * version, stashes the host vtable, and dispatches to each module's
 * registrar.
 */

#include "PluginGlobals.hpp"

#include <entt.hpp>

namespace mtype_entt
{
    const MTypePluginHost* g_host = nullptr;

    HandleRegistry<entt::registry> g_registries;
    HandleRegistry<EnttView>       g_views;
}

extern "C" MTYPE_PLUGIN_EXPORT
int mtype_plugin_register(uint32_t hostAbiVersion,
                          const MTypePluginHost* host,
                          MTypeContext* registrationCtx)
{
    if (hostAbiVersion != MTYPE_PLUGIN_ABI_VERSION) {
        return 1;
    }
    mtype_entt::g_host = host;

    mtype_entt::registerRegistryNatives(registrationCtx);
    mtype_entt::registerComponentNatives(registrationCtx);
    mtype_entt::registerViewNatives(registrationCtx);
    mtype_entt::registerSignalNatives(registrationCtx);
    mtype_entt::registerCtxNatives(registrationCtx);
    return 0;
}
