#pragma once
/*
 * Shared helpers for every binding TU.
 *
 * Each .cpp picks its own exception-type string ("EnttError") via the kEx
 * constant in its anonymous namespace so call sites stay terse.
 *
 * ScopedCtx is the bridge that lets signal trampolines (which run inside
 * EnTT's emplace/destroy from inside a plugin native) recover the current
 * MTypeContext* and call back into mType via host->callFunction.
 */

#include "PluginGlobals.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mtype_entt::detail
{
    /* ---- Per-native ctx stack for signal callbacks. ---- */

    inline thread_local std::vector<MTypeContext*> g_ctxStack;

    struct ScopedCtx
    {
        explicit ScopedCtx(MTypeContext* c) { g_ctxStack.push_back(c); }
        ~ScopedCtx()                        { g_ctxStack.pop_back(); }
        ScopedCtx(const ScopedCtx&) = delete;
        ScopedCtx& operator=(const ScopedCtx&) = delete;
    };

    inline MTypeContext* currentCtx()
    {
        return g_ctxStack.empty() ? nullptr : g_ctxStack.back();
    }

    /* ---- Arity / type-tag guards. ---- */

    inline bool requireArgs(MTypeContext* ctx, int argc, int expected,
                            const char* name, const char* exType)
    {
        if (argc != expected) {
            std::string m = std::string(name) + ": expected " + std::to_string(expected)
                          + " args, got " + std::to_string(argc);
            g_host->raiseError(ctx, exType, m.c_str());
            return false;
        }
        return true;
    }

    inline bool requireArgsAtLeast(MTypeContext* ctx, int argc, int atLeast,
                                    const char* name, const char* exType)
    {
        if (argc < atLeast) {
            std::string m = std::string(name) + ": expected at least " + std::to_string(atLeast)
                          + " args, got " + std::to_string(argc);
            g_host->raiseError(ctx, exType, m.c_str());
            return false;
        }
        return true;
    }

    /* ---- Scalar extraction shorthands. ---- */

    inline const char* getStr(const MTypeValue* v, std::size_t* outLen = nullptr)
    {
        if (g_host->getTag(v) != MT_TAG_STRING) {
            if (outLen) *outLen = 0;
            return "";
        }
        return g_host->getString(v, outLen);
    }
    inline float   getF(const MTypeValue* v) { return static_cast<float>(g_host->getFloat(v)); }
    inline int64_t getI(const MTypeValue* v) { return g_host->getInt(v); }
    inline bool    getB(const MTypeValue* v) { return g_host->getBool(v) != 0; }

    /* ---- Handle lookup with auto-raise. ---- */

    template <typename T>
    T* findOrRaise(HandleRegistry<T>& reg, int64_t id,
                   MTypeContext* ctx, const char* op, const char* exType)
    {
        T* p = reg.find(id);
        if (!p) {
            std::string m = std::string(op) + ": invalid handle id "
                          + std::to_string(id);
            g_host->raiseError(ctx, exType, m.c_str());
        }
        return p;
    }

    /* ---- Batched prefix registrar. ---- */

    struct Registrar
    {
        MTypeContext* ctx;
        const char*   prefix;

        Registrar& operator()(const char* suffix, MTypeNativeFn fn)
        {
            std::string name = std::string(prefix) + suffix;
            g_host->registerFunction(ctx, name.c_str(), fn, nullptr);
            return *this;
        }
    };
}
