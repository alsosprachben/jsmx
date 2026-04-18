// Repo-authored string iterator parity fixture.
//
// String iteration yields Unicode code points, NOT UTF-16
// units. Multi-byte UTF-8 chars and surrogate pairs must
// round-trip as single code-point strings — a JS engine that
// iterated by UTF-16 unit would split surrogate pairs into
// two invalid halves.

// ASCII: 3 single-byte code points.
// for (const c of "abc") — c takes "a", "b", "c".

// Multi-byte: "é" is 2 UTF-8 bytes (C3 A9) but 1 code point.
// for (const c of "café") — c takes "c", "a", "f", "é".

// Surrogate pair: "🎉" (U+1F389) is a single astral code
// point (4 UTF-8 bytes F0 9F 8E 89). A correct iterator
// yields exactly one step, not two.
// for (const c of "🎉") — c takes "🎉" exactly once.
