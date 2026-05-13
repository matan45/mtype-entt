/*
 * Registry context-var bindings.
 *
 * EnTT's registry.ctx() container stores singletons keyed by C++ type. We
 * expose it to scripts as a string-keyed map of CtxVar tagged unions
 * (one per registry) living in PerRegistryState::ctxVars. This is simpler
 * than mapping every script-side ctx_set into entt::any per-call and lets
 * scripts use intuitive name keys.
 */

#include "BindingHelpers.hpp"
#include "ScriptComponent.hpp"

#include <entt.hpp>

namespace mtype_entt
{
    namespace
    {
        constexpr const char* kEx = "EnttError";

        bool grabReg(MTypeContext* ctx, const MTypeValue* arg,
                      const char* op, entt::registry** outReg)
        {
            *outReg = detail::findOrRaise(g_registries, detail::getI(arg), ctx, op, kEx);
            return *outReg != nullptr;
        }

        std::string keyOf(const MTypeValue* v)
        {
            std::size_t len = 0;
            const char* s = g_host->getString(v, &len);
            return std::string(s ? s : "", len);
        }

        MTypeValue* nCtxSetInt(void*, MTypeContext* ctx,
                                 const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_set_int";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx)) return g_host->makeVoid(ctx);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeVoid(ctx);
            CtxVar v;
            v.tag = FieldTag::Int;
            v.i   = detail::getI(args[2]);
            stateOf(*reg).ctxVars[keyOf(args[1])] = std::move(v);
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nCtxSetFloat(void*, MTypeContext* ctx,
                                   const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_set_float";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx)) return g_host->makeVoid(ctx);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeVoid(ctx);
            CtxVar v;
            v.tag = FieldTag::Float;
            v.f   = g_host->getFloat(args[2]);
            stateOf(*reg).ctxVars[keyOf(args[1])] = std::move(v);
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nCtxSetBool(void*, MTypeContext* ctx,
                                  const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_set_bool";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx)) return g_host->makeVoid(ctx);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeVoid(ctx);
            CtxVar v;
            v.tag = FieldTag::Bool;
            v.b   = detail::getB(args[2]);
            stateOf(*reg).ctxVars[keyOf(args[1])] = std::move(v);
            return g_host->makeVoid(ctx);
        }

        MTypeValue* nCtxSetString(void*, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_set_string";
            if (!detail::requireArgs(ctx, argc, 3, op, kEx)) return g_host->makeVoid(ctx);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeVoid(ctx);
            CtxVar v;
            v.tag = FieldTag::String;
            std::size_t len = 0;
            const char* s = g_host->getString(args[2], &len);
            v.s.assign(s ? s : "", len);
            stateOf(*reg).ctxVars[keyOf(args[1])] = std::move(v);
            return g_host->makeVoid(ctx);
        }

        const CtxVar* findVar(entt::registry& reg, const std::string& k)
        {
            auto& m = stateOf(reg).ctxVars;
            auto it = m.find(k);
            return it == m.end() ? nullptr : &it->second;
        }

        MTypeValue* nCtxGetInt(void*, MTypeContext* ctx,
                                 const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_get_int";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeInt(ctx, 0);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeInt(ctx, 0);
            const CtxVar* v = findVar(*reg, keyOf(args[1]));
            if (!v) return g_host->makeInt(ctx, 0);
            switch (v->tag) {
                case FieldTag::Int:    return g_host->makeInt(ctx, v->i);
                case FieldTag::Float:  return g_host->makeInt(ctx, static_cast<int64_t>(v->f));
                case FieldTag::Bool:   return g_host->makeInt(ctx, v->b ? 1 : 0);
                case FieldTag::String: return g_host->makeInt(ctx, 0);
            }
            return g_host->makeInt(ctx, 0);
        }

        MTypeValue* nCtxGetFloat(void*, MTypeContext* ctx,
                                   const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_get_float";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeFloat(ctx, 0.0);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeFloat(ctx, 0.0);
            const CtxVar* v = findVar(*reg, keyOf(args[1]));
            if (!v) return g_host->makeFloat(ctx, 0.0);
            switch (v->tag) {
                case FieldTag::Float:  return g_host->makeFloat(ctx, v->f);
                case FieldTag::Int:    return g_host->makeFloat(ctx, static_cast<double>(v->i));
                case FieldTag::Bool:   return g_host->makeFloat(ctx, v->b ? 1.0 : 0.0);
                case FieldTag::String: return g_host->makeFloat(ctx, 0.0);
            }
            return g_host->makeFloat(ctx, 0.0);
        }

        MTypeValue* nCtxGetBool(void*, MTypeContext* ctx,
                                  const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_get_bool";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeBool(ctx, 0);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeBool(ctx, 0);
            const CtxVar* v = findVar(*reg, keyOf(args[1]));
            if (!v) return g_host->makeBool(ctx, 0);
            switch (v->tag) {
                case FieldTag::Bool:   return g_host->makeBool(ctx, v->b ? 1 : 0);
                case FieldTag::Int:    return g_host->makeBool(ctx, v->i != 0 ? 1 : 0);
                case FieldTag::Float:  return g_host->makeBool(ctx, v->f != 0.0 ? 1 : 0);
                case FieldTag::String: return g_host->makeBool(ctx, !v->s.empty() ? 1 : 0);
            }
            return g_host->makeBool(ctx, 0);
        }

        MTypeValue* nCtxGetString(void*, MTypeContext* ctx,
                                    const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_get_string";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeString(ctx, "", 0);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeString(ctx, "", 0);
            const CtxVar* v = findVar(*reg, keyOf(args[1]));
            if (!v) return g_host->makeString(ctx, "", 0);
            switch (v->tag) {
                case FieldTag::String: return g_host->makeString(ctx, v->s.c_str(), v->s.size());
                case FieldTag::Int: {
                    std::string s = std::to_string(v->i);
                    return g_host->makeString(ctx, s.c_str(), s.size());
                }
                case FieldTag::Float: {
                    std::string s = std::to_string(v->f);
                    return g_host->makeString(ctx, s.c_str(), s.size());
                }
                case FieldTag::Bool:
                    return g_host->makeString(ctx, v->b ? "true" : "false", v->b ? 4 : 5);
            }
            return g_host->makeString(ctx, "", 0);
        }

        MTypeValue* nCtxHas(void*, MTypeContext* ctx,
                              const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_has";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeBool(ctx, 0);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeBool(ctx, 0);
            return g_host->makeBool(ctx, findVar(*reg, keyOf(args[1])) ? 1 : 0);
        }

        MTypeValue* nCtxUnset(void*, MTypeContext* ctx,
                                const MTypeValue* const* args, int argc)
        {
            detail::ScopedCtx _cx(ctx);
            const char* op = "__native__entt_ctx_unset";
            if (!detail::requireArgs(ctx, argc, 2, op, kEx)) return g_host->makeVoid(ctx);
            entt::registry* reg = nullptr;
            if (!grabReg(ctx, args[0], op, &reg)) return g_host->makeVoid(ctx);
            stateOf(*reg).ctxVars.erase(keyOf(args[1]));
            return g_host->makeVoid(ctx);
        }
    }

    void registerCtxNatives(MTypeContext* ctx)
    {
        detail::Registrar r{ctx, "__native__entt_"};
        r("ctx_set_int",    &nCtxSetInt)
         ("ctx_set_float",  &nCtxSetFloat)
         ("ctx_set_bool",   &nCtxSetBool)
         ("ctx_set_string", &nCtxSetString)
         ("ctx_get_int",    &nCtxGetInt)
         ("ctx_get_float",  &nCtxGetFloat)
         ("ctx_get_bool",   &nCtxGetBool)
         ("ctx_get_string", &nCtxGetString)
         ("ctx_has",        &nCtxHas)
         ("ctx_unset",      &nCtxUnset);
    }
}
