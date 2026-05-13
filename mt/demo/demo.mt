// mtype-entt demo: 100-entity Position+Velocity simulation.
//
// Builds a registry, registers two script-defined components (Position
// and Velocity), spawns 100 entities with deterministic seeded values,
// then runs 60 ticks of `pos += vel * dt` over a view. Prints the final
// position of entity 50 and the total entity count.

import * from "../lib/Entt.mt";

__plugin_load("mt/mtype_entt.dll");

class Position {
    public float x;
    public float y;
}

class Velocity {
    public float dx;
    public float dy;
}

Registry reg = Entts::createRegistry();

string[] posFields = ["x", "y"];
int[]    posTags   = [Entts::fieldFloat(), Entts::fieldFloat()];
reg.registerComponent("Position", "Position", posFields, posTags);

string[] velFields = ["dx", "dy"];
int[]    velTags   = [Entts::fieldFloat(), Entts::fieldFloat()];
reg.registerComponent("Velocity", "Velocity", velFields, velTags);

// Spawn 100 entities with deterministic Position+Velocity.
int[] entities = new int[100];
int   i        = 0;
while (i < 100) {
    int e = reg.create();
    Position p = new Position();
    p.x = (float)i;
    p.y = (float)i * 2.0;
    reg.emplace(e, "Position", p);

    Velocity v = new Velocity();
    v.dx = 0.1 + 0.01 * (float)i;
    v.dy = -0.05;
    reg.emplace(e, "Velocity", v);

    entities[i] = e;
    i = i + 1;
}

// 60 ticks of pos += vel * dt.
float dt = 0.016;
int   tick = 0;
string[] both = ["Position", "Velocity"];
while (tick < 60) {
    View v = reg.view(both);
    int e = v.next();
    while (e != 0) {
        Position p = (Position) reg.get(e, "Position");
        Velocity vel = (Velocity) reg.get(e, "Velocity");
        p.x = p.x + vel.dx * dt;
        p.y = p.y + vel.dy * dt;
        reg.emplace(e, "Position", p);
        e = v.next();
    }
    v.destroy();
    tick = tick + 1;
}

int sample = entities[50];
Position pf = (Position) reg.get(sample, "Position");
print("alive=" + reg.size());
print("entity 50 final: (" + pf.x + ", " + pf.y + ")");

reg.destroy();
__plugin_unload("mt/mtype_entt.dll");
