// mType wrapper around the mtype-entt native plugin (EnTT ECS bindings).
//
// Components are script-defined: register by name + the mType class to
// round-trip with + the declared field shape. Use Entts::fieldInt(),
// fieldFloat(), fieldBool(), fieldString() as the tag constants.
//
// IMPORTANT: the plugin's `get()` synthesizes return objects via the host's
// makeObject — the mType-side constructor body is NOT invoked. Component
// classes should therefore have plain public fields and rely on defaults
// (or no constructor logic) for the path values come back from get().
// Input objects passed to emplace() are constructed normally by the
// script, so they can use any constructor you like.

class View {
    public int handle;
    public constructor(int h) { this.handle = h; }
    public function destroy(): void { __native__entt_view_destroy(this.handle); }

    // Eager snapshot. Allocates an int[] in the current call's arena.
    public function entities(): int[] {
        return __native__entt_view_entities(this.handle);
    }

    // One-at-a-time iteration. Returns 0 when exhausted. The snapshot
    // is taken on first call so the iteration is stable even if scripts
    // emplace/remove components inside the loop body.
    public function next(): int {
        return __native__entt_view_next(this.handle);
    }

    // Re-arm next() for another pass.
    public function reset(): void {
        __native__entt_view_reset(this.handle);
    }
}

class Registry {
    public int handle;
    public constructor(int h) { this.handle = h; }
    public function destroy(): void { __native__entt_registry_destroy(this.handle); }
    public function clear(): void { __native__entt_registry_clear(this.handle); }
    public function size(): int { return __native__entt_registry_size(this.handle); }

    // ---- entity lifecycle ----
    public function create(): int { return __native__entt_create(this.handle); }
    public function destroyEntity(int e): void { __native__entt_destroy_entity(this.handle, e); }
    public function valid(int e): bool { return __native__entt_valid(this.handle, e); }

    // ---- component schema ----
    public function registerComponent(string name, string className,
                                       string[] fields, int[] fieldTags): void {
        __native__entt_register_component(this.handle, name, className, fields, fieldTags);
    }
    // Empty-payload component (existence flag only).
    public function registerTag(string name): void {
        string[] f = new string[0];
        int[]    t = new int[0];
        __native__entt_register_component(this.handle, name, "", f, t);
    }

    // ---- component ops ----
    public function emplace(int e, string comp, object value): void {
        __native__entt_emplace(this.handle, e, comp, value);
    }
    public function emplaceTag(int e, string comp): void {
        __native__entt_emplace_tag(this.handle, e, comp);
    }
    public function get(int e, string comp): object {
        return __native__entt_get(this.handle, e, comp);
    }
    public function tryGet(int e, string comp): object {
        return __native__entt_try_get(this.handle, e, comp);
    }
    public function has(int e, string comp): bool {
        return __native__entt_has(this.handle, e, comp);
    }
    public function allOf(int e, string[] comps): bool {
        return __native__entt_all_of(this.handle, e, comps);
    }
    public function anyOf(int e, string[] comps): bool {
        return __native__entt_any_of(this.handle, e, comps);
    }
    public function remove(int e, string comp): void {
        __native__entt_remove(this.handle, e, comp);
    }
    public function patch(int e, string comp, object value): void {
        __native__entt_patch(this.handle, e, comp, value);
    }

    // ---- queries ----
    public function view(string[] include): View {
        return new View(__native__entt_view(this.handle, include));
    }
    public function viewExcluding(string[] include, string[] exclude): View {
        return new View(__native__entt_view_excluding(this.handle, include, exclude));
    }

    // ---- signals. fn = name of a top-level mType function with
    // signature `function fn(int registryHandle, int entity): void`. ----
    public function onConstruct(string comp, string fn): void {
        __native__entt_on_construct(this.handle, comp, fn);
    }
    public function onUpdate(string comp, string fn): void {
        __native__entt_on_update(this.handle, comp, fn);
    }
    public function onDestroy(string comp, string fn): void {
        __native__entt_on_destroy(this.handle, comp, fn);
    }

    // ---- registry-scoped ctx vars (string-keyed) ----
    public function ctxSetInt(string k, int v): void   { __native__entt_ctx_set_int   (this.handle, k, v); }
    public function ctxSetFloat(string k, float v): void { __native__entt_ctx_set_float(this.handle, k, v); }
    public function ctxSetBool(string k, bool v): void  { __native__entt_ctx_set_bool  (this.handle, k, v); }
    public function ctxSetString(string k, string v): void { __native__entt_ctx_set_string(this.handle, k, v); }
    public function ctxGetInt(string k): int       { return __native__entt_ctx_get_int   (this.handle, k); }
    public function ctxGetFloat(string k): float   { return __native__entt_ctx_get_float (this.handle, k); }
    public function ctxGetBool(string k): bool     { return __native__entt_ctx_get_bool  (this.handle, k); }
    public function ctxGetString(string k): string { return __native__entt_ctx_get_string(this.handle, k); }
    public function ctxHas(string k): bool         { return __native__entt_ctx_has(this.handle, k); }
    public function ctxUnset(string k): void       { __native__entt_ctx_unset(this.handle, k); }
}

class Entts {
    public static function createRegistry(): Registry {
        return new Registry(__native__entt_registry_create());
    }

    // Field-tag constants. Values MUST match the FieldTag enum in
    // src/ScriptComponent.hpp (Int=0, Float=1, Bool=2, String=3).
    public static function fieldInt():    int { return 0; }
    public static function fieldFloat():  int { return 1; }
    public static function fieldBool():   int { return 2; }
    public static function fieldString(): int { return 3; }
}
