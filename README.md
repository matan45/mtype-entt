# mtype-entt

EnTT bindings for [mType](https://github.com/...). A runtime plugin that
exposes [EnTT](https://github.com/skypjack/entt)'s ECS (entities,
components, views, signals) to mType scripts.

Components are **script-defined**: register a component by name + the
mType class to round-trip with + the field shape. The plugin bridges
mType objects ↔ EnTT storage at runtime; you do not need to recompile
the C++ side to add new components.

## Build

Requires CMake ≥ 3.16 and a C++17 toolchain. EnTT 3.16 is vendored as
the single-header `vendor/entt/entt.hpp`; no external dependencies.

```
cmake -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/mtype_entt.dll` (Windows) /
`build/mtype_entt.so` (Linux) / `build/mtype_entt.dylib` (macOS).

Copy the artifact into a location mType can resolve from `__plugin_load`:

```
cp build/Release/mtype_entt.dll mt/
```

## Run the demos

```
mType.exe mt/demo/smoke.mt          # registry + entity lifecycle
mType.exe mt/demo/demo.mt           # 100-entity Position+Velocity sim
mType.exe mt/demo/demo_signals.mt   # on_construct / on_destroy callbacks
mType.exe mt/demo/demo_view.mt      # view include + exclude filters
```

## Quick example

```mt
import * from "mt/lib/Entt.mt";

__plugin_load("mt/mtype_entt.dll");

class Position { public float x; public float y; }
class Velocity { public float dx; public float dy; }

Registry reg = Entts::createRegistry();

string[] posF = ["x", "y"];
string[] velF = ["dx", "dy"];
int[]    fF   = [Entts::fieldFloat(), Entts::fieldFloat()];
reg.registerComponent("Position", "Position", posF, fF);
reg.registerComponent("Velocity", "Velocity", velF, fF);

int e = reg.create();
Position p = new Position(); p.x = 0.0; p.y = 0.0;
Velocity v = new Velocity(); v.dx = 1.0; v.dy = 0.5;
reg.emplace(e, "Position", p);
reg.emplace(e, "Velocity", v);

string[] q = ["Position", "Velocity"];
View view = reg.view(q);
int it = view.next();
while (it != 0) {
    Position cur = (Position) reg.get(it, "Position");
    Velocity vel = (Velocity) reg.get(it, "Velocity");
    cur.x = cur.x + vel.dx;
    cur.y = cur.y + vel.dy;
    reg.emplace(it, "Position", cur);
    it = view.next();
}
view.destroy();

reg.destroy();
__plugin_unload("mt/mtype_entt.dll");
```

## API surface

`Registry` (one int handle per `entt::registry`):

| Group | Methods |
|---|---|
| Lifecycle | `destroy()`, `clear()`, `size()` |
| Entities | `create()`, `destroyEntity(e)`, `valid(e)` |
| Schema | `registerComponent(name, className, fields, fieldTags)`, `registerTag(name)` |
| Components | `emplace`, `emplaceTag`, `get`, `tryGet`, `has`, `allOf`, `anyOf`, `remove`, `patch` |
| Queries | `view(include)`, `viewExcluding(include, exclude)` |
| Signals | `onConstruct(name, fn)`, `onUpdate(name, fn)`, `onDestroy(name, fn)` |
| Ctx vars | `ctxSet/Get{Int,Float,Bool,String}`, `ctxHas`, `ctxUnset` |

`View`: `entities() → int[]`, `next() → int` (0 when done), `reset()`,
`destroy()`. Snapshot is taken on first `next()` so iteration is stable
even if you mutate components inside the loop.

`Entts`: static factory + field-tag constants
(`fieldInt`, `fieldFloat`, `fieldBool`, `fieldString`).

## How dynamic components work

The C ABI between mType and a plugin only passes ints, floats, bools,
strings, fixed-size arrays, and class instances — no C++ templates.
EnTT's API, on the other hand, is heavily templated. The bridge:

- One C++ struct (`ScriptComponent`) backs every script-registered
  component. It holds a `std::vector<FieldValue>` (tagged-union per
  field).
- Each registered component name is hashed to an `entt::id_type`. EnTT
  supports multiple distinct pools of the **same** C++ type keyed by
  `id_type`: `registry.storage<ScriptComponent>(id)` returns a separate
  pool per name.
- Queries use `entt::runtime_view`, which accepts type-erased storages
  obtained via `registry.storage(id)`. `view`/`viewExcluding` resolve
  names to ids and feed the matching pools in.
- Signals connect through `registry.on_construct<ScriptComponent>(id)`.
  The listener is a member function on `SignalListener` (stored in a
  per-registry `std::list` for stable addresses). Inside the listener we
  recover the current `MTypeContext*` from a thread-local
  `ScopedCtx` stack and call back into mType via
  `host->callFunction(ctx, fnName, [registry, entity])`.

See `src/ScriptComponent.hpp` for the bridge data structures and
`src/ComponentBindings.cpp` for the emplace/get/view machinery.

## Project layout

```
include/PluginHostApi.h        mType plugin C ABI v2 (vendored, keep in sync)
src/PluginEntry.cpp            plugin entrypoint + cross-TU globals
src/PluginGlobals.hpp          extern decls + register*Natives prototypes
src/HandleRegistry.hpp         int64-keyed C++ pointer registry
src/BindingHelpers.hpp         Registrar, requireArgs, ScopedCtx, getF/getI/getB
src/ScriptComponent.{hpp,cpp}  ScriptComponent + buildFromObject / writeIntoObject
src/RegistryBindings.cpp       registry_create/destroy/clear/size, entity ops
src/ComponentBindings.cpp      register_component, emplace/get/has/remove/patch
src/ViewBindings.cpp           view, view_excluding, view_next, view_entities
src/SignalBindings.cpp         on_construct/update/destroy
src/CtxBindings.cpp            ctx_set/get/has/unset for int/float/bool/string
mt/lib/Entt.mt                 Registry, View, Entts mType wrappers
mt/demo/*.mt                   end-to-end test scripts
vendor/entt/entt.hpp           EnTT 3.16 single-header release
```

## Caveats

- The mType-side constructor body is **not** invoked when the plugin
  synthesizes an object from `get()` (the host's `makeObject` allocates
  a bare instance and the plugin populates fields directly). Component
  classes used as round-trip targets should have plain public fields
  and avoid constructor side-effects.
- Signal callbacks must be top-level mType functions, resolved by name
  with signature `function fn(int registryHandle, int entity): void`.
- `runtime_view` iteration in `entt::runtime_view` does not preserve a
  particular order across calls; rely on entity presence, not ordering.
- Strings in component fields are copied into the plugin on `emplace`
  and copied back out on `get`. For hot loops with large strings,
  prefer numeric component data.
