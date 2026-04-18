// Repo-authored WinterTC standard base64 parity fixture.
//
// Exercises the jsval_base64_encode / jsval_base64_decode pair
// on typical inputs: ASCII bytes, bytes that force the "+" and
// "/" alphabet positions, and decode round-trips.

const enc_1 = btoa("\x66\x6f\x6f");                  // "Zm9v"
const enc_2 = btoa("\xfb\xff\xff");                  // "+///"  — uses +, /

// Decode round-trips
const back_1 = atob("Zm9v");                          // [0x66, 0x6f, 0x6f]
const back_2 = atob("+///");                          // [0xfb, 0xff, 0xff]

// In the jsmx shape:
//   btoa(<Uint8Array>) => jsval_base64_encode(region, arr, &string)
//   atob(<string>)     => jsval_base64_decode(region, str, &arr)
