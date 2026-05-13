/*
 * Implementation of the mType-object ↔ ScriptComponent bridge.
 */

#include "ScriptComponent.hpp"
#include "BindingHelpers.hpp"

namespace mtype_entt
{
    namespace
    {
        constexpr const char* kEx = "EnttError";

        /* Drop a value into a FieldValue per the declared tag. Coerces
         * int<->float lightly so scripts don't have to be picky. */
        bool coerceInto(FieldValue& out, FieldTag tag,
                         const MTypeValue* v, MTypeContext* ctx,
                         const std::string& fieldName, const char* op)
        {
            out.tag = tag;
            const MTypeTag vt = g_host->getTag(v);
            switch (tag) {
                case FieldTag::Int:
                    if (vt == MT_TAG_INT)        out.i = g_host->getInt(v);
                    else if (vt == MT_TAG_FLOAT) out.i = static_cast<int64_t>(g_host->getFloat(v));
                    else if (vt == MT_TAG_BOOL)  out.i = g_host->getBool(v) ? 1 : 0;
                    else {
                        std::string m = std::string(op) + ": field '" + fieldName + "' expected int";
                        g_host->raiseError(ctx, kEx, m.c_str());
                        return false;
                    }
                    return true;
                case FieldTag::Float:
                    if (vt == MT_TAG_FLOAT)    out.f = g_host->getFloat(v);
                    else if (vt == MT_TAG_INT) out.f = static_cast<double>(g_host->getInt(v));
                    else {
                        std::string m = std::string(op) + ": field '" + fieldName + "' expected float";
                        g_host->raiseError(ctx, kEx, m.c_str());
                        return false;
                    }
                    return true;
                case FieldTag::Bool:
                    if (vt == MT_TAG_BOOL)     out.b = g_host->getBool(v) != 0;
                    else if (vt == MT_TAG_INT) out.b = g_host->getInt(v) != 0;
                    else {
                        std::string m = std::string(op) + ": field '" + fieldName + "' expected bool";
                        g_host->raiseError(ctx, kEx, m.c_str());
                        return false;
                    }
                    return true;
                case FieldTag::String:
                    if (vt == MT_TAG_STRING) {
                        std::size_t len = 0;
                        const char* s = g_host->getString(v, &len);
                        out.s.assign(s, len);
                    } else {
                        std::string m = std::string(op) + ": field '" + fieldName + "' expected string";
                        g_host->raiseError(ctx, kEx, m.c_str());
                        return false;
                    }
                    return true;
            }
            return false;
        }
    }

    PerRegistryState& stateOf(entt::registry& reg)
    {
        // ctx().emplace<T>() inserts if absent; otherwise returns existing.
        auto& ctx = reg.ctx();
        if (auto* p = ctx.find<PerRegistryState>()) return *p;
        return ctx.emplace<PerRegistryState>();
    }

    const ComponentSchema* resolveSchema(entt::registry& reg,
                                          const std::string& name,
                                          MTypeContext* ctx,
                                          const char* op)
    {
        auto& st = stateOf(reg);
        auto it = st.schemasByName.find(name);
        if (it == st.schemasByName.end()) {
            std::string m = std::string(op) + ": unknown component '" + name + "'";
            g_host->raiseError(ctx, kEx, m.c_str());
            return nullptr;
        }
        return &it->second;
    }

    ScriptComponent buildFromObject(const ComponentSchema& schema,
                                     const MTypeValue* value,
                                     MTypeContext* ctx)
    {
        ScriptComponent comp;
        comp.fields.resize(schema.fieldNames.size());

        // Tag component — no fields to read.
        if (schema.fieldNames.empty()) return comp;

        if (g_host->getTag(value) != MT_TAG_OBJECT) {
            std::string m = "emplace: expected object value for component '"
                          + schema.name + "'";
            g_host->raiseError(ctx, kEx, m.c_str());
            return comp;
        }

        for (std::size_t i = 0; i < schema.fieldNames.size(); ++i) {
            const std::string& fn = schema.fieldNames[i];
            MTypeValue* f = g_host->objGet(ctx, value, fn.c_str());
            if (!f) {
                std::string m = "emplace: object missing field '" + fn
                              + "' for component '" + schema.name + "'";
                g_host->raiseError(ctx, kEx, m.c_str());
                return comp;
            }
            if (!coerceInto(comp.fields[i], schema.fieldTags[i], f, ctx, fn, "emplace")) {
                return comp;
            }
        }
        return comp;
    }

    MTypeValue* writeIntoObject(const ComponentSchema& schema,
                                 const ScriptComponent& comp,
                                 MTypeContext* ctx)
    {
        // Tag component or unspecified className — return null so scripts
        // can treat presence-only components without an instance.
        if (schema.className.empty()) {
            return g_host->makeNull(ctx);
        }

        MTypeValue* obj = g_host->makeObject(ctx, schema.className.c_str());
        if (!obj) {
            std::string m = "get: failed to instantiate class '"
                          + schema.className + "' for component '"
                          + schema.name + "'";
            g_host->raiseError(ctx, kEx, m.c_str());
            return g_host->makeNull(ctx);
        }

        const std::size_t n = std::min(schema.fieldNames.size(), comp.fields.size());
        for (std::size_t i = 0; i < n; ++i) {
            const FieldValue& fv = comp.fields[i];
            MTypeValue* v = nullptr;
            switch (fv.tag) {
                case FieldTag::Int:    v = g_host->makeInt(ctx, fv.i);                       break;
                case FieldTag::Float:  v = g_host->makeFloat(ctx, fv.f);                     break;
                case FieldTag::Bool:   v = g_host->makeBool(ctx, fv.b ? 1 : 0);              break;
                case FieldTag::String: v = g_host->makeString(ctx, fv.s.c_str(), fv.s.size()); break;
            }
            g_host->objSet(obj, schema.fieldNames[i].c_str(), v);
        }
        return obj;
    }
}
