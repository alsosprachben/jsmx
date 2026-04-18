// Repo-authored WinterTC standard base64 error-path fixture.
//
// Standard base64 decode rejects:
//   - base64url-only characters ("-", "_")
//   - inputs whose length is not a multiple of 4

let threw_minus = false;
try { atob("ab-d"); } catch (e) { threw_minus = true; }  // '-' not in standard alphabet

let threw_underscore = false;
try { atob("ab_d"); } catch (e) { threw_underscore = true; }  // '_' not in standard alphabet

let threw_short = false;
try { atob("Zm9"); } catch (e) { threw_short = true; }  // length 3 not multiple of 4

// jsmx: jsval_base64_decode returns -1 for each of these.
