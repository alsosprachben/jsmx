// Repo-authored WinterTC base64url parity fixture.
//
// Base64url (RFC 4648 §5, no padding) differs from standard
// base64 at indices 62 and 63: uses "-" and "_" instead of "+"
// and "/", and emits no "=" padding. Commonly used for JWT
// segments and WebCrypto JWK key material.

// Bytes that force indices 62 and 63 in the output:
//   0xf8 → "-"  (010-000)
//   0xfb → "+"  (standard; base64url remaps to "-")
// [0xfb, 0xef, 0xff]  → standard "++//"
//                    → base64url "--__"

// Round-trip verifies the alphabet remap and the absence of
// padding. 1-byte input: [0x66] → "Zg" (no "==" padding).
