
JSMX
====

`jsmx` (pronounced Jazz Max) is a fork of `jsmn`, a minimalist JSON parser in C. `jsmx` provides a maximalist interface with the same minimalist design of jsmn. It is the logical conclusion to jsmn, with the following additions:
 1. The token elements are extended with parent links, enabling a full DOM traversal, providing...
 2. A full DOM interface, providing...
 3. In-line editing, which means that...
 4. An emitter API is provided.
 5. UTF-8 with compliant JSON quoting and UTF-8 or UTF-32 wide character interfaces. This is natively provided, either depending on no library, or depending on libc.

See [README-jsmn.md] for `jsmn`.
