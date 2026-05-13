// Minimal smoke test: load plugin, create + destroy a registry.

import * from "../lib/Entt.mt";

__plugin_load("mt/mtype_entt.dll");

Registry reg = Entts::createRegistry();
int e = reg.create();
print("created entity " + e);
print("valid? " + reg.valid(e));
print("size=" + reg.size());
reg.destroyEntity(e);
print("after destroy, valid? " + reg.valid(e));
print("size=" + reg.size());
reg.destroy();

__plugin_unload("mt/mtype_entt.dll");
print("ok");
