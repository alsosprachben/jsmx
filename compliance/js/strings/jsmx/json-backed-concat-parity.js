/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx. The intended lowering is:
 *
 *   1. parse JSON so the receiver and one argument stay JSON-backed
 *   2. run concat through the jsval bridge with exact measure/execute sizing
 *   3. compare the results against equivalent native-string operands
 *
 * The generated C fixture demonstrates that parity directly.
 */
