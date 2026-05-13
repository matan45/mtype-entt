#pragma once
/*
 * Bridge between mType objects (script-defined) and EnTT's compile-time
 * templated storage. One C++ struct (ScriptComponent) backs every
 * script-registered component; pools are distinguished by entt::id_type so
 * registry.storage<ScriptComponent>(id) gives a separate pool per component
 * name. See https://github.com/skypjack/entt/wiki/Crash-Course:-entity-component-system
 * for the multi-storage-per-type pattern.
 *
 * Schemas (mType class name + field names + tags) live in PerRegistryState,
 * which we store inside entt::registry::ctx() so it auto-dies with its
 * owning registry.
 */

#include "PluginHostApi.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt.hpp>

namespace mtype_entt
{
    /* Type tag for a script-component field. Public ABI values — mType
     * scripts pass these as ints; do not renumber. */
    enum class FieldTag : int {
        Int    = 0,
        Float  = 1,
        Bool   = 2,
        String = 3
    };

    struct FieldValue
    {
        FieldTag tag = FieldTag::Int;
        int64_t  i   = 0;
        double   f   = 0.0;
        bool     b   = false;
        std::string s;
    };

    struct ScriptComponent
    {
        std::vector<FieldValue> fields;
    };

    struct ComponentSchema
    {
        std::string              name;        // hashed -> id below
        std::string              className;   // mType class for round-trip; "" = tag
        std::vector<std::string> fieldNames;
        std::vector<FieldTag>    fieldTags;
        entt::id_type            id = 0;      // entt::hashed_string{name}.value()
    };

    struct CtxVar
    {
        FieldTag tag = FieldTag::Int;
        int64_t  i   = 0;
        double   f   = 0.0;
        bool     b   = false;
        std::string s;
    };

    /* Sits inside registry.ctx(). One per registry.
     * Move-only: contains stable references handed to sigh listeners, so
     * accidental copies would be dangerous and entt::ctx never needs to
     * copy a stored value. */
    struct PerRegistryState
    {
        std::unordered_map<std::string, ComponentSchema> schemasByName;
        std::unordered_map<entt::id_type, std::string>   nameById;
        std::unordered_map<std::string, CtxVar>          ctxVars;

        PerRegistryState() = default;
        PerRegistryState(const PerRegistryState&)            = delete;
        PerRegistryState& operator=(const PerRegistryState&) = delete;
        PerRegistryState(PerRegistryState&&) noexcept            = default;
        PerRegistryState& operator=(PerRegistryState&&) noexcept = default;
    };

    /* Helpers — implementation in ScriptComponent.cpp. */

    /* Look up (or report missing) per-registry state. Always present after
     * registry creation, but keep the guard for safety. */
    PerRegistryState& stateOf(entt::registry& reg);

    /* Resolve a component name to its schema. Returns nullptr if unknown
     * and raises EnttError. */
    const ComponentSchema* resolveSchema(entt::registry& reg,
                                          const std::string& name,
                                          MTypeContext* ctx,
                                          const char* op);

    /* Build a ScriptComponent by reading each declared field off `value`
     * (an mType MT_TAG_OBJECT). Missing fields default per tag (0 / 0.0 /
     * false / ""). Raises on type mismatch. */
    ScriptComponent buildFromObject(const ComponentSchema& schema,
                                     const MTypeValue* value,
                                     MTypeContext* ctx);

    /* Construct a fresh mType object for the schema's className and
     * populate its fields from `comp`. Returns null if className is empty
     * (tag component) or makeObject fails. */
    MTypeValue* writeIntoObject(const ComponentSchema& schema,
                                 const ScriptComponent& comp,
                                 MTypeContext* ctx);
}
