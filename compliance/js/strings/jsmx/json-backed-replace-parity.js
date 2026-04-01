/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx. The intended lowering is:
 *
 *   1. parse JSON so the receiver, search string, and replacement stay
 *      JSON-backed on the first path
 *   2. run replace through the jsval bridge for both plain-string and
 *      regex-backed search values
 *   3. compare the results against equivalent native-string operands
 *
 * The generated C fixture demonstrates that parity directly.
 */
