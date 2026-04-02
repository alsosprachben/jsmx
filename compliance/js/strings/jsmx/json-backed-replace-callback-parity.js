/*
 * Repo-authored jsmx compliance source.
 *
 * This case exists because test262 does not express the parsed-JSON-backed
 * versus native-string boundary that matters to jsmx for function replacers.
 * The generated fixture compares callback-based replace/replaceAll behavior
 * across parsed and native strings on the jsval bridge directly.
 */
