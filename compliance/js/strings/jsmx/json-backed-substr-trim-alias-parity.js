/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx, and the official
 * trimLeft / trimRight directories only cover metadata and alias identity.
 * The intended lowering is:
 *
 *   1. parse JSON so the strings stay JSON-backed
 *   2. run substr / trimLeft / trimRight through the jsval bridge
 *   3. compare the results against equivalent native-string operands
 *
 * The generated C fixture demonstrates that parity directly.
 */
