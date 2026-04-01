/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx. The intended lowering is:
 *
 *   1. parse JSON so the receiver string stays JSON-backed
 *   2. run split through the jsval bridge with both string and regex separators
 *   3. compare the arrays against equivalent native-string operands
 *
 * The generated C fixture demonstrates that parity directly.
 */
