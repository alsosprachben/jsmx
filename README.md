# JSMX

`jsmx` (pronounced Jazz Max) is a C runtime support library for transpiling
JavaScript to C.

It started as a fork of `jsmn`, but the project direction is broader:

- expose C-idiomatic interfaces
- preserve JavaScript-facing semantics where they matter

In practice, that means transpiled code should be able to call into this
library for behavior that is awkward to lower into plain C alone:

- parsed token trees with parent and DOM-style traversal support
- page-resident JS values, objects, arrays, and strings over caller-owned
  buffers
- in-place editing and emitter support for transformed structures
- JavaScript-oriented string handling with UTF-8 and UTF-32 interfaces
- Unicode case mapping, normalization, and collation data
- URL helpers that fit web-facing runtime behavior

## Direction

This repository is building the runtime substrate around a JS-to-C transpiler.

- The transpiler lowers syntax into C calls.
- `jsmx` provides the semantic helpers those calls target.
- The aim is not to mimic JavaScript syntax in C.
- The aim is to provide runtime primitives that let generated C preserve
  JavaScript behavior.

That makes semantic correctness more important than surface familiarity:

- new APIs should feel natural in C
- those APIs should still be designed around the JavaScript rules the
  transpiled program expects

## Repository Layout

- `jsmn.c`, `jsmn.h`
  - parser core and token model
- `jsnum.c`, `jsnum.h`
  - internal libc-optional numeric parsing, formatting, remainder, and
    integer-coercion helpers shared by `jsval`
- `jsregex.c`, `jsregex.h`, `jsmx_config.h`
  - optional backend-gated regex layer
  - the first backend is PCRE2
  - the JS-facing regex/runtime surface remains UTF-16-oriented
  - a narrow `jsregex8_*` layer is available for UTF-8-oriented production
    tools that only need low-level compile / exec / search
  - default regex construction stays non-JIT
  - explicit `jsregex_compile_utf16_jit(...)` and `jsval_regexp_new_jit(...)`
    entrypoints let generated code choose JIT for longer-lived regexes
  - translator-facing generated code should only choose the JIT constructor
    when the regex is clearly hoisted and reused across repeated matches
  - the current JS-facing slice covers:
    - regex-backed `search`
    - `exec`
    - `match`
    - `matchAll`
    - named capture groups across exec-shaped results and regex replace-family
      substitutions
    - `RegExp.prototype.test`
    - semantic `source`
    - core regex state helpers, including repeated `lastIndex` and exec-result
      fidelity
  - translator-owned regex compatibility choices live in
    `skills/jsmx-transpile-tests/`
- `jsval.c`, `jsval.h`
  - versioned page-set storage with an in-page root handle
  - native and JSON-backed JS value/object/array representations
  - explicit promotion helpers for generated C
  - shallow capacity-planned promotion for selectively mutating parsed JSON
    subtrees
  - JSON emission for JSON-compatible values
  - explicit native object/array helpers for:
    - own-property checks
    - ordered key/value access
    - shallow own-property copy
    - fresh own-property clone into a new native object
    - fresh dense-array clone into a new native array
    - bounded dense-array splice edits with removed-result arrays
    - translator-visible object / array spread-style lowerings built from
      clone, copy, and append recipes
    - delete
    - push
    - pop
    - shift
    - unshift
    - dense length writes
  - primitive `typeof`, nullish detection, numeric, arithmetic, equality, and
    relational helpers for flattened generated code
  - translator-facing callback replacers for `replace` / `replaceAll`,
    including regex named-group `groups` arguments
  - narrow `/u` literal rewrite helpers for regex-backed string methods,
    including:
    - lone-surrogate atoms
    - fixed no-capture sequences
    - simple no-capture literal-member classes
    - simple no-capture negated literal-member classes
    - simple no-capture ranged classes
    - simple no-capture negated ranged classes
    - simple no-capture predefined classes `\\d`, `\\D`, `\\s`, `\\S`,
      `\\w`, and `\\W`
    - `matchAll`
  - a direct bridge from `jsval_t` into the thin `jsmethod` string-builtins
    layer
  - explicit two-phase `normalize` measurement/execution helpers
- `jsmethod.c`, `jsmethod.h`
  - thin JS-method helpers layered over `jsstr` / `unicode`
  - covers:
    - receiver coercion
    - Symbol rejection
    - typed method errors
    - `String.prototype.normalize`
    - `concat`
    - `replace`
    - `replaceAll`
    - case conversion
    - trim / repeat plus the legacy trim aliases
    - padding
    - non-regex search / membership including `split`
    - the first regex-backed `search` entrypoint
    - accessor helpers
    - substring extraction including `substr`
    - `toWellFormed`-style behavior
  - includes exact `normalize` sizing for deterministic values
- `jsstr.c`, `jsstr.h`
  - string and Unicode-facing helpers
  - explicit `NFC` / `NFD` / `NFKC` / `NFKD` normalization APIs with
    NFC-preserving wrappers
- `unicode.c`, `unicode.h`, `unicode_*.h`
  - generated Unicode databases
  - normalization-form parsing
  - lookup logic
- `jsurl.c`, `jsurl.h`
  - URL utilities
- `docs/flattening-boundary.md`
  - the contract between `jsmx`, the translator, and any future slow path
- `compliance/`
  - committed JS compliance sources
  - generated C fixtures
  - the manifest / contract used by the generated-test workflow
  - current suites:
    - `strings`
    - `regex`
    - `values`
    - `objects`
- `bench/`
  - dedicated runtime benchmark corpus for Node versus committed generated C
  - local JSON timing output and manifest-driven pairing
- `example/`
  - small example programs
- `runtime_modules/`
  - reusable host-module bridges for transpiled production programs
  - shared implementations plus runtime-specific exposure profiles such as
    Node and WinterTC
- `test/` and `test_*.c`
  - parser tests
  - generated-C smoke tests
  - feature-specific tests
- `scripts/`
  - generators for Unicode-derived headers
- `skills/jsmx-transpile-tests/`
  - repo-local guidance for translating real JS compliance tests into committed
    `jsmx` C fixtures

## Build And Test

Use `make` for the standard workflow.

### Core targets

- `make`
  - build the library, examples, and tests
- `make test`
  - run the parser test matrix

### Focused runtime targets

- `make test_jsnum`
  - run the internal numeric parse / format / remainder / integer-coercion
    tests that back `jsval`
- `make test_jsregex`
  - run the optional regex backend tests directly when `JSMX_WITH_REGEX=1`
    is enabled
- `make test_jsval`
  - run the page-resident value / object storage tests, including the `jsval`
    to `jsmethod` bridge
- `make test_jsmethod`
  - run the thin JS-method coercion and normalize tests
- `make test_codegen`
  - run the generated-C smoke harness and formalized string compliance slices
- `make test_jsstr`, `make test_unicode`, `make test_jsurl`,
  `make test_utf8`, `make test_collation`
  - run focused feature tests

### Compliance

- `make test_compliance`
  - compile and run committed generated compliance fixtures listed in
    `compliance/manifest.json`
  - the runner builds one shared runtime archive per invocation
  - fixture compiles then fan out in parallel
  - `JOBS=N` or `COMPLIANCE_JOBS=N` override worker count
- `make bench`
  - run the dedicated runtime benchmark suite against Node and compiled
    generated C
  - emit local JSON results under `bench/results/latest.json` by default
  - optional environment overrides:
    - `BENCH_FILTER`
    - `BENCH_ITERATIONS`
    - `BENCH_WARMUP`
    - `BENCH_OUTPUT`
    - `BENCH_NODE`

Generated compliance fixtures are checked in under `compliance/generated/`.

- Translation is an authoring workflow guided by `skills/jsmx-transpile-tests/`.
- It is not part of `make` or CI.
- `make test_compliance` reads `compliance/manifest.json`.
- The runner checks fixture exit codes against the manifest contract.
- The runner understands feature-gated entries through `required_features`.
- Regex-backed slices run only when:
  - `JSMX_WITH_REGEX=1` is present in `CFLAGS`, or
  - `JSMX_FEATURES=regex` is set

The committed corpus currently spans:

- string built-ins
  - normalization
  - `concat`
  - `replace`
  - `replaceAll`
  - callback replacers for `replace` / `replaceAll`, including regex
    named-group `groups` arguments
  - case conversion
  - well-formedness
  - trim methods:
    - `trim`
    - `trimStart`
    - `trimEnd`
    - legacy aliases `trimLeft` / `trimRight`
  - `repeat`
  - padding:
    - `padStart`
    - `padEnd`
  - non-regex search core:
    - `indexOf`
    - `lastIndexOf`
    - `includes`
    - `startsWith`
    - `endsWith`
    - `split`
  - feature-gated regex-backed:
    - `search`
    - `replace`
    - `replaceAll`
    - `split`
    - `exec`
    - `match`
    - `matchAll`
  - accessor methods:
    - `charAt`
    - `at`
    - `charCodeAt`
    - `codePointAt`
  - substring extraction:
    - `slice`
    - `substring`
    - `substr`
- a feature-gated `regex` suite
  - semantic `RegExp.prototype.source`
  - `RegExp.prototype.test`
  - `RegExp.prototype[Symbol.matchAll]`
  - supported flag / state access
  - constructor-state behavior
  - deeper repeated `exec` / `test` / `match` state fidelity
  - named capture groups and `groups` objects on exec-shaped results
- official and repo-authored `jsval` value semantics
  - primitive `typeof`
  - nullish-coalescing parity
  - ternary truthiness / identity parity
  - test262 strict-equality
  - primitive abstract equality / inequality
  - logical-operator slices:
    - `!`
    - `&&`
    - `||`
  - primitive numeric coercion / arithmetic
  - primitive integer coercion plus bitwise and shift slices:
    - `~`
    - `&`
    - `|`
    - `^`
    - `<<`
    - `>>`
    - `>>>`
  - primitive relational-operator slices:
    - `<`
    - `<=`
    - `>`
    - `>=`
  - explicit JSON-backed-to-native promotion patterns
  - shallow capacity-planned promotion for selected native rewrites inside
    parsed trees
  - native object / array helpers such as:
    - own-property checks
    - ordered key / value access
    - shallow own-property copy
    - object spread / `Object.assign`-style fresh clone and merge
    - array spread-style fresh clone and append
    - delete
    - overwrite
    - push
    - pop
    - shift
    - unshift
    - observable dense length writes

Translation policy:

- official cases should normally use a real upstream source file plus one
  idiomatic generated translation
- when a file mixes flattenable checks with already-understood boundary
  behavior, the translation should preserve the whole file through an idiomatic
  slow path rather than omitting those checks
- repo-authored fixtures are reserved for `jsmx`-specific contracts such as:
  - JSON-backed parity
  - planned-capacity behavior
- for regex-bearing cases, the translator should make the compatibility choice
  up front using the repo-local regex guide:
  - direct-lowered
  - rewrite-backed
  - idiomatic slow path
  - unsupported
  - manifest notes should say so explicitly

Current boundaries:

- arrays remain dense and capacity-bounded
- hole and sparse-index semantics are outside the current flattened slice

The string layer also supports explicit normalization-form selection for
generated code.

- `jsstr*_normalize()` remains the NFC default
- lower layers expose exact normalization sizing helpers such as:
  - `unicode_normalize_form_workspace_len()`
  - `jsstr*_normalize_form_needed()`
- generated C can therefore measure first and then provide exact caller-owned
  memory

The `jsmethod` layer carries that pattern upward through:

- `jsmethod_string_normalize_measure()`
- `jsmethod_string_normalize_into()`
- `jsmethod_string_concat_measure()` / `jsmethod_string_concat()`
- `jsmethod_string_replace_measure()` / `jsmethod_string_replace()`
- `jsmethod_string_replace_all_measure()` / `jsmethod_string_replace_all()`
- `jsmethod_string_repeat_measure()` / `jsmethod_string_repeat()`
- `jsmethod_string_pad_start_measure()` / `jsmethod_string_pad_start()`
- `jsmethod_string_pad_end_measure()` / `jsmethod_string_pad_end()`

`jsval` mirrors that pattern through:

- `jsval_method_string_normalize_measure()` /
  `jsval_method_string_normalize()`
- `jsval_method_string_concat_measure()` / `jsval_method_string_concat()`
- `jsval_method_string_replace_measure()` / `jsval_method_string_replace()`
- `jsval_method_string_replace_all_measure()` /
  `jsval_method_string_replace_all()`
- translator-facing callback ABI:
  - `jsval_method_string_replace_fn()`
  - `jsval_method_string_replace_all_fn()`
- `jsval_method_string_repeat_measure()` / `jsval_method_string_repeat()`
- `jsval_method_string_pad_start_measure()` /
  `jsval_method_string_pad_start()`
- `jsval_method_string_pad_end_measure()` / `jsval_method_string_pad_end()`

## Numeric Backends

Numeric parsing, formatting, and remainder are routed through the internal
`jsnum` layer.

- The default build avoids `libm`-only assumptions.
- `USE_LIBC` keeps the libc-backed path available.

To run the same matrix explicitly through libc, use commands such as:

```sh
make CFLAGS='-DUSE_LIBC' LDLIBS='-lm' \
  test_jsnum test_jsval test_codegen test_compliance test
```

## Optional Regex Support

Optional regex support is available through:

- [CMakeLists.txt](/home/ben/repos/jsmx/CMakeLists.txt)
- [vcpkg.json](/home/ben/repos/jsmx/vcpkg.json)

With `VCPKG_ROOT` set, configure with:

```sh
cmake -S . -B build -DJSMX_ENABLE_REGEX=ON
```

The CMake path will:

- resolve `pcre2` through `vcpkg`
- define `JSMX_WITH_REGEX=1`
- define `JSMX_REGEX_BACKEND_PCRE2=1`

## Boundary

The boundary in [`docs/flattening-boundary.md`](docs/flattening-boundary.md)
is intentional.

- `jsmx` should stay the flattened semantic layer
- the translator decides:
  - when to lower directly
  - when to emit an explicit slow path
  - when to reject unsupported semantics

The compliance manifest records that distinction separately from pass / fail
execution status:

- `lowering_class`
  - why the case fits, or does not fit, the flattened layer
- `translation_mode`
  - whether the fixture is:
    - `idiomatic_flattened`
    - `idiomatic_slow_path`

See [the jsmn README](README-jsmn.md) for upstream parser background.
