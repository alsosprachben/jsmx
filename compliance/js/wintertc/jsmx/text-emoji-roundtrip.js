// Repo-authored WinterTC TextEncoder/TextDecoder emoji
// round-trip fixture.
//
// 4-byte UTF-8 characters (outside the BMP) must survive an
// encode → decode cycle byte-exact and string-equal.

const enc = new TextEncoder();
const dec = new TextDecoder();

const source = "🎉";  // U+1F389 PARTY POPPER
const bytes = enc.encode(source);
// bytes === new Uint8Array([0xF0, 0x9F, 0x8E, 0x89])

const restored = dec.decode(bytes);
// restored === source (string equality)
