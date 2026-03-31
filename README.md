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
- `jsnum.c`, `jsnum.h`: internal libc-optional numeric parsing, formatting, remainder, and integer-coercion helpers shared by `jsval`
- `jsval.c`, `jsval.h`: versioned page-set storage with an in-page root handle, plus native and JSON-backed JS value/object/array representations, explicit promotion helpers for generated C, shallow capacity-planned promotion for selectively mutating parsed JSON subtrees, JSON emission for JSON-compatible values, explicit native object/array helpers such as own-property checks, delete, push, and dense length writes, primitive numeric/arithmetic/equality/relational helpers for flattened generated code, and a direct bridge from `jsval_t` into the thin `jsmethod` string-builtins layer, including explicit two-phase `normalize` measurement/execution helpers
- `jsmethod.c`, `jsmethod.h`: thin JS-method helpers layered over `jsstr`/`unicode` for receiver coercion, Symbol rejection, typed method errors, and `String.prototype.normalize`, case-conversion, non-regex search/membership, accessor, substring-extraction, and `toWellFormed`-style behavior, including exact `normalize` sizing for deterministic values
- `jsstr.c`, `jsstr.h`: string and Unicode-facing helpers, including explicit `NFC`/`NFD`/`NFKC`/`NFKD` normalization APIs with NFC-preserving wrappers
- `unicode.c`, `unicode.h`, `unicode_*.h`: generated Unicode databases, normalization-form parsing, and lookup logic
- `mnurl.c`, `mnurl.h`: URL utilities
- `docs/flattening-boundary.md`: the contract between `jsmx`, the translator, and any future slow path
- `compliance/`: committed JS compliance sources, generated C fixtures, and the manifest/contract used by the generated-test workflow across `strings`, `values`, and `objects` suites
- `example/`: small example programs
- `test/` and `test_*.c`: parser, generated-C smoke tests, and feature-specific tests
- `scripts/`: generators for Unicode-derived headers
- `skills/jsmx-transpile-tests/`: repo-local guidance for translating real JS compliance tests into committed `jsmx` C fixtures

## Build And Test

Use `make` for the standard workflow:

- `make`: build the library, examples, and tests
- `make test`: run the parser test matrix
- `make test_jsnum`: run the internal numeric parse/format/remainder/integer-coercion tests that back `jsval`
- `make test_jsval`: run the page-resident value/object storage tests, including the `jsval` to `jsmethod` bridge
- `make test_jsmethod`: run the thin JS-method coercion and normalize tests
- `make test_codegen`: run the generated-C smoke harness and formalized string compliance slice
- `make test_compliance`: compile and run committed generated compliance fixtures listed in `compliance/manifest.json` in parallel; the runner builds one shared runtime archive per invocation and then compiles each fixture against it, and `JOBS=N` or `COMPLIANCE_JOBS=N` override worker count
- `make test_jsstr`, `make test_unicode`, `make test_mnurl`, `make test_utf8`, `make test_collation`: run focused feature tests

Generated compliance fixtures are checked in under `compliance/generated/`. The translation workflow is an authoring step guided by `skills/jsmx-transpile-tests/`; it is not part of `make` or CI. `make test_compliance` reads `compliance/manifest.json`, compiles each listed fixture against the runtime sources, and checks the fixture exit code against the manifest contract. The committed corpus now spans string built-ins, including official and repo-authored coverage for normalization, case conversion, well-formedness, the non-regex search core (`indexOf`, `lastIndexOf`, `includes`, `startsWith`, `endsWith`), accessor methods (`charAt`, `at`, `charCodeAt`, `codePointAt`), and substring extraction (`slice`, `substring`), official and repo-authored `jsval` value semantics including test262 strict-equality and primitive abstract equality/inequality, logical-operator slices (`!`, `&&`, `||`), primitive numeric coercion/arithmetic slices, primitive integer coercion plus bitwise and shift slices (`~`, `&`, `|`, `^`, `<<`, `>>`, `>>>`), and primitive relational-operator slices (`<`, `<=`, `>`, `>=`) across both numeric and string ordering paths, explicit JSON-backed-to-native promotion patterns, shallow capacity-planned promotion for selected native rewrites inside parsed trees, and native object/array helpers such as own-property checks, delete, overwrite, push, and observable dense length writes. Official cases should normally use a real upstream source file plus one idiomatic generated translation; when a file mixes flattenable checks with already-understood boundary behavior, the translation should preserve the whole file through an idiomatic slow path rather than omitting those checks. Repo-authored fixtures are reserved for `jsmx`-specific contracts such as JSON-backed parity or planned-capacity behavior. Arrays remain dense and capacity-bounded in this layer; hole and sparse-index semantics are still outside the current flattened slice. The string layer supports explicit normalization-form selection for generated code while keeping `jsstr*_normalize()` as the NFC default, and the lower layers expose exact normalization sizing helpers such as `unicode_normalize_form_workspace_len()` and `jsstr*_normalize_form_needed()` so generated C can measure first and then provide exact caller-owned memory. The `jsmethod` layer carries that pattern upward through `jsmethod_string_normalize_measure()` and `jsmethod_string_normalize_into()`, while `jsval` mirrors it with `jsval_method_string_normalize_measure()` and `jsval_method_string_normalize()` for page-resident generated C.

Numeric parsing, formatting, and remainder are now routed through the internal `jsnum` layer. The default build avoids `libm`-only assumptions, while `USE_LIBC` keeps the libc-backed path available. To run the same matrix explicitly through libc, use commands such as `make CFLAGS='-DUSE_LIBC' LDLIBS='-lm' test_jsnum test_jsval test_codegen test_compliance test`.

The boundary in [`docs/flattening-boundary.md`](docs/flattening-boundary.md) is intentional: `jsmx` should stay the flattened semantic layer, while the translator decides when to lower directly, when to emit an explicit slow path, and when to reject unsupported semantics. The compliance manifest now records that distinction separately from pass/fail execution status through a `lowering_class` field, and also records whether each fixture is an `idiomatic_flattened` or `idiomatic_slow_path` translation.

See [the jsmn README](README-jsmn.md) for upstream parser background.
