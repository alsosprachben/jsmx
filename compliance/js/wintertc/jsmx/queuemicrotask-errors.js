// Repo-authored WinterTC queueMicrotask error-path fixture.
//
// queueMicrotask(callback) must reject non-function callbacks
// with a TypeError in spec; jsmx surfaces this as a -1 return
// from jsval_queue_microtask.

let threw_num = false;
try { queueMicrotask(42); } catch (e) { threw_num = true; }

let threw_undef = false;
try { queueMicrotask(undefined); } catch (e) { threw_undef = true; }
