/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx for regex-backed function
 * replacers. The generated fixture compares parsed and native receivers on the
 * jsval regex replace callback path directly.
 */
