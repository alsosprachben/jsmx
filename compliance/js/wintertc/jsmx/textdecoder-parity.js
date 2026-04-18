// Repo-authored WinterTC TextDecoder parity fixture.
//
// TextDecoder.decode(buffer) interprets the buffer's bytes as
// UTF-8 and returns the corresponding string. Accepts a
// Uint8Array or an ArrayBuffer in this slice.

const dec = new TextDecoder();
const enc = new TextEncoder();

// Round-trip: encode then decode yields the original string.
const ascii_s = dec.decode(enc.encode("hello"));        // "hello"
const mb_s = dec.decode(enc.encode("café"));             // "café"

// Decode from a raw ArrayBuffer (bytes set up manually).
const ab = new ArrayBuffer(5);
const view = new Uint8Array(ab);
view.set([0x77, 0x6f, 0x72, 0x6c, 0x64]);
const ab_s = dec.decode(ab);                             // "world"
