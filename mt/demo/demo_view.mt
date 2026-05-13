// mtype-entt demo: view include + exclude filters.
//
// Three components: A, B, C. Six entities with various combinations.
// view([A, B]) -> entities with both A and B.
// viewExcluding([A], [C]) -> entities with A and not C.

import * from "../lib/Entt.mt";

__plugin_load("mt/mtype_entt.dll");

class Box { public int n; }

Registry reg = Entts::createRegistry();
reg.registerComponent("A", "Box", ["n"], [Entts::fieldInt()]);
reg.registerComponent("B", "Box", ["n"], [Entts::fieldInt()]);
reg.registerComponent("C", "Box", ["n"], [Entts::fieldInt()]);

function spawn(Registry r, bool a, bool b, bool c, int n): int {
    int e = r.create();
    Box bx = new Box(); bx.n = n;
    if (a) { r.emplace(e, "A", bx); }
    if (b) { r.emplace(e, "B", bx); }
    if (c) { r.emplace(e, "C", bx); }
    return e;
}

int eAB   = spawn(reg, true,  true,  false, 1);
int eABC  = spawn(reg, true,  true,  true,  2);
int eA    = spawn(reg, true,  false, false, 3);
int eAC   = spawn(reg, true,  false, true,  4);
int eB    = spawn(reg, false, true,  false, 5);
int eC    = spawn(reg, false, false, true,  6);

// Has A and B.
string[] ab_q = ["A", "B"];
View ab = reg.view(ab_q);
int[] abEnts = ab.entities();
ab.destroy();
print("A & B count=" + abEnts.length);   // expect 2 (eAB, eABC)

// Has A, excludes C.
string[] inc_a = ["A"];
string[] exc_c = ["C"];
View aNotC = reg.viewExcluding(inc_a, exc_c);
int[] ents = aNotC.entities();
aNotC.destroy();
print("A & !C count=" + ents.length);    // expect 2 (eAB, eA)

reg.destroy();
__plugin_unload("mt/mtype_entt.dll");
