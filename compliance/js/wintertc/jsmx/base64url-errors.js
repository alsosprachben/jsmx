// Repo-authored WinterTC base64url error-path fixture.
//
// Base64url decode rejects:
//   - standard-alphabet characters ("+", "/") — use base64 for those
//   - inputs with length-mod-4 == 1 (never a valid base64 encoding)

let threw_plus = false;
try { atob_url("ab+d"); } catch (e) { threw_plus = true; }  // '+' not in base64url

let threw_slash = false;
try { atob_url("ab/d"); } catch (e) { threw_slash = true; }  // '/' not in base64url

let threw_short = false;
try { atob_url("a"); } catch (e) { threw_short = true; }  // length 1 never valid
