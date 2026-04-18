// Repo-authored WinterTC standard base64 padding fixture.
//
// Standard base64 outputs are always length-mod-4: 1-byte
// payloads pad with "==", 2-byte payloads with "=". Decode
// must accept both.

const one_byte   = btoa("\x66");       // "Zg=="
const two_bytes  = btoa("\x66\x6f");   // "Zm8="

const back_one   = atob("Zg==");       // [0x66]
const back_two   = atob("Zm8=");       // [0x66, 0x6f]
