# JSMX

`jsmx` (pronounced Jazz Max) is a C runtime support library for transpiling JavaScript to C. It started as a fork of `jsmn`, but the project direction is broader: expose C-idiomatic interfaces while preserving JavaScript-facing semantics where they matter.

In practice, that means transpiled code should be able to call into this library for behavior that is awkward to lower into plain C alone:

- parsed token trees with parent and DOM-style traversal support
- page-resident JS values, objects, arrays, and strings over caller-owned buffers
- in-place editing and emitter support for transformed structures
- JavaScript-oriented string handling with UTF-8 and UTF-32 interfaces
- Unicode case mapping, normalization, and collation data
- URL helpers that fit web-facing runtime behavior

## Direction

This repository is building the runtime substrate around a JS-to-C transpiler. The transpiler can lower syntax into C calls; `jsmx` is where the semantic helpers live. The aim is not to mimic JavaScript syntax in C, but to provide runtime primitives that let generated C preserve JavaScript behavior.

That makes semantic correctness more important than surface familiarity. New APIs should feel natural in C, but they should be designed around the JavaScript rules the transpiled program expects.

## Repository Layout

- `jsmn.c`, `jsmn.h`: parser core and token model
- `jsval.c`, `jsval.h`: versioned page-set storage with an in-page root handle, plus native and JSON-backed JS value/object/array representations, explicit promotion helpers for generated C, and JSON emission for JSON-compatible values
- `jsmethod.c`, `jsmethod.h`: thin JS-method helpers layered over `jsstr`/`unicode` for receiver coercion, Symbol rejection, typed method errors, and `String.prototype.normalize`/case-conversion/`toWellFormed`-style behavior
- `jsstr.c`, `jsstr.h`: string and Unicode-facing helpers, including explicit `NFC`/`NFD`/`NFKC`/`NFKD` normalization APIs with NFC-preserving wrappers
- `unicode.c`, `unicode.h`, `unicode_*.h`: generated Unicode databases, normalization-form parsing, and lookup logic
- `mnurl.c`, `mnurl.h`: URL utilities
- `compliance/`: committed JS compliance sources, generated C fixtures, and the manifest/contract used by the generated-test workflow
- `example/`: small example programs
- `test/` and `test_*.c`: parser, generated-C smoke tests, and feature-specific tests
- `scripts/`: generators for Unicode-derived headers
- `skills/jsmx-transpile-tests/`: repo-local guidance for translating real JS compliance tests into committed `jsmx` C fixtures

## Build And Test

Use `make` for the standard workflow:

- `make`: build the library, examples, and tests
- `make test`: run the parser test matrix
- `make test_jsval`: run the page-resident value/object storage tests
- `make test_jsmethod`: run the thin JS-method coercion and normalize tests
- `make test_codegen`: run the generated-C smoke harness and formalized string compliance slice
- `make test_compliance`: compile and run committed generated compliance fixtures listed in `compliance/manifest.json`
- `make test_jsstr`, `make test_unicode`, `make test_mnurl`, `make test_utf8`, `make test_collation`: run focused feature tests

Generated compliance fixtures are checked in under `compliance/generated/`. The translation workflow is an authoring step guided by `skills/jsmx-transpile-tests/`; it is not part of `make` or CI. `make test_compliance` reads `compliance/manifest.json`, compiles each listed fixture against the runtime sources, and checks the fixture exit code against the manifest contract. The string layer now supports explicit normalization-form selection for generated code while keeping `jsstr*_normalize()` as the NFC default, and the thin `jsmethod` layer covers the JS-facing string-method boundary where receiver/form coercion, locale-tag coercion, and Symbol rejection matter.

See [the jsmn README](README-jsmn.md) for upstream parser background.
