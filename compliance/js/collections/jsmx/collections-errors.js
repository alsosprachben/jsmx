// Repo-authored Set / Map error-path fixture.
//
// jsmx's Set and Map are fixed-capacity hash containers —
// adding beyond cap returns -1 with errno == ENOBUFS. Growing
// requires jsval_set_clone / jsval_map_clone with a larger
// capacity.

// Set capacity overflow.
const s = new Set();
// s.add("x"); s.add("y"); s.add("z");  // jsmx: add #3 returns -1 if cap was 2
// jsval_set_clone(cap=4) creates a fresh, grown set.

// Map capacity overflow: same shape.

// Accessors on a non-Set / non-Map receiver fail.
