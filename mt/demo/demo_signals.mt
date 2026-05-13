// mtype-entt demo: on_construct / on_destroy signals.
//
// Connects mType functions to component lifecycle events and verifies the
// callbacks fire synchronously from inside emplace/destroyEntity.

import * from "../lib/Entt.mt";

__plugin_load("mt/mtype_entt.dll");

class Position {
    public float x;
    public float y;
}

int spawnCount = 0;
int despawnCount = 0;

function logSpawn(int reg, int e): void {
    spawnCount = spawnCount + 1;
    print("  spawn callback: entity=" + e);
}
function logDespawn(int reg, int e): void {
    despawnCount = despawnCount + 1;
    print("  despawn callback: entity=" + e);
}

Registry reg = Entts::createRegistry();
string[] posFields = ["x", "y"];
int[]    posTags   = [Entts::fieldFloat(), Entts::fieldFloat()];
reg.registerComponent("Position", "Position", posFields, posTags);

reg.onConstruct("Position", "logSpawn");
reg.onDestroy("Position", "logDespawn");

int e1 = reg.create();
int e2 = reg.create();
int e3 = reg.create();
Position p = new Position();
p.x = 1.0; p.y = 2.0;
reg.emplace(e1, "Position", p);
reg.emplace(e2, "Position", p);
reg.emplace(e3, "Position", p);

reg.remove(e2, "Position");
reg.destroyEntity(e3);

print("spawnCount=" + spawnCount + " despawnCount=" + despawnCount);

reg.destroy();
__plugin_unload("mt/mtype_entt.dll");
