#pragma once
/*
 * Cross-TU plugin globals. g_host is set by mtype_plugin_register and read
 * by every binding TU. The HandleRegistry instances are defined in
 * PluginEntry.cpp.
 */

#include "PluginHostApi.h"
#include "HandleRegistry.hpp"

#include <entt.hpp>

namespace mtype_entt
{
    extern const MTypePluginHost* g_host;

    extern HandleRegistry<entt::registry> g_registries;

    struct EnttView;  // defined in ViewBindings.cpp
    extern HandleRegistry<EnttView> g_views;

    void registerRegistryNatives(MTypeContext* ctx);
    void registerComponentNatives(MTypeContext* ctx);
    void registerViewNatives(MTypeContext* ctx);
    void registerSignalNatives(MTypeContext* ctx);
    void registerCtxNatives(MTypeContext* ctx);
}
