// Repo-authored WinterTC TextEncoder parity fixture.
//
// TextEncoder.encode(s) produces a Uint8Array holding the
// UTF-8 bytes of s. UTF-8 is the only encoding the spec
// mandates and the only one jsmx supports.

const enc = new TextEncoder();

// ASCII: "hello" → [0x68, 0x65, 0x6c, 0x6c, 0x6f] (5 bytes).
const ascii = enc.encode("hello");

// Multi-byte: "café" → 5 bytes (é = 0xC3 0xA9).
const mb = enc.encode("café");

// 4-byte UTF-8: "🎉" (U+1F389) → [0xF0, 0x9F, 0x8E, 0x89].
const party = enc.encode("🎉");

// Each result is a Uint8Array whose byteLength matches the
// UTF-8 length of the source string, and whose bytes are the
// UTF-8 encoding of the source.
