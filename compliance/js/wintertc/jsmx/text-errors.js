// Repo-authored WinterTC TextEncoder/TextDecoder error-path
// fixture.
//
// encode() takes a string; passing a non-string is a spec
// TypeError. decode() takes a BufferSource (Uint8Array /
// ArrayBuffer); passing anything else is a spec TypeError.
// jsmx surfaces these as -1 return codes from the C API.

const enc = new TextEncoder();
const dec = new TextDecoder();

// encode(42) → TypeError in spec, -1 in jsmx.
let threw_enc = false;
try { enc.encode(42); } catch (e) { threw_enc = true; }
// threw_enc === true (in spec); jsmx_text_encode_utf8 returns -1.

// decode(42) → TypeError in spec, -1 in jsmx.
let threw_dec = false;
try { dec.decode(42); } catch (e) { threw_dec = true; }
// threw_dec === true (in spec); jsmx_text_decode_utf8 returns -1.
