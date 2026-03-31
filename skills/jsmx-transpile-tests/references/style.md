# Translation Style

This document defines how JS compliance tests should be translated into `jsmx`-style C.

## Goal

Translate the semantic intent of the JS test into reviewable, self-contained C that exercises the `jsmx` runtime surfaces directly.

The output is not a generic C translation. It is a `jsmx`-targeted fixture.

## Source Policy

- Prefer a real upstream official source file when the suite expresses the
  behavior you want to prove.
- For official-source cases, commit one idiomatic generated translation by
  default. Do not keep a second literal witness fixture unless the literal
  structure itself needs review.
- Use repo-authored sources only when the behavior is specifically about the
  flattened `jsmx` contract rather than the upstream language/library
  contract, such as:
  - JSON-backed parity
  - planned-capacity behavior
  - shallow promotion policy
  - page-resident identity or locality concerns
- If an upstream file mixes flattenable checks with behavior that already has
  a known slow-path contract, preserve the whole file through one idiomatic
  generated translation rather than omitting those checks.
- Only preserve a flattenable subset and document omissions when the missing
  behavior does not yet have a clear slow-path contract.

## API Selection

- Use `jsstr` for pure string semantics:
  - normalization
  - case mapping
  - UTF-8 / UTF-16 / UTF-32 length behavior
  - concatenation
  - substring or inclusion checks
  - well-formedness conversion
- Use `jsmethod` for JS string built-ins that need receiver or argument
  coercion:
  - normalize
  - trimming (`trim`, `trimStart`, `trimEnd`, `trimLeft`, `trimRight`)
  - repetition (`repeat`)
  - padding (`padStart`, `padEnd`)
  - case conversion
  - search/membership (`indexOf`, `lastIndexOf`, `includes`, `startsWith`,
    `endsWith`)
  - accessors (`charAt`, `at`, `charCodeAt`, `codePointAt`)
  - substring extraction (`slice`, `substring`, `substr`)
  - well-formedness methods
- Use `jsval` when the test involves:
  - JS value tags
  - truthiness
  - strict equality
  - JSON-backed values
  - object or array access
  - promotion from JSON-backed to native
  - JSON emission through `jsval_copy_json()`

## Memory Model

- Prefer caller-owned stack or page-backed buffers.
- Use `*_init_from_buf()` forms for strings.
- Use `jsval_region_init()` for page-resident values.
- Do not add `malloc`, `realloc`, or `free` to generated fixtures.

## Promotion Policy

- JSON-backed `jsval` values are readable but not JS-mutable.
- On first semantic mutation, use:
  - `jsval_region_promote_root()` for the in-page root
  - `jsval_promote_in_place()` for a local slot
- Do not hide promotion inside mutation helpers.

## Unsupported Semantics

If the source test depends on behavior not yet supported by `jsmx`, classify it using `docs/flattening-boundary.md` and return `KNOWN_UNSUPPORTED` through the shared contract header when the current repo cannot execute it directly.

Examples:

- prototype chain semantics
- descriptors or accessors
- symbols
- functions or closures
- promises
- sparse-array behavior
- `JSON.stringify`-specific handling of `undefined` or holes

Do not approximate unsupported behavior just to preserve the source test structure.

## Lowering Classification

Every manifest entry should declare one `lowering_class`:

- `static_pass`
  - the fixture is expected to run entirely through current `jsmx` / `jsmethod` APIs
- `slow_path_needed`
  - the semantics are valid JS, but a correct lowering needs a translator-emitted dynamic slow path outside current `jsmx`
- `unsupported`
  - the behavior is intentionally out of scope for the current project

`expected_status` is the runtime outcome for the current fixture. `lowering_class` explains where that fixture belongs relative to the flattening boundary.

Every manifest entry should also declare one `translation_mode`:

- `idiomatic_flattened`
  - the fixture demonstrates the intended flattened lowering into `jsmx` / `jsmethod`
- `idiomatic_slow_path`
  - the fixture demonstrates the intended translator-emitted slow path outside the flattened layer
- `literal`
  - the fixture intentionally mirrors the upstream test structure more directly

Prefer `idiomatic_flattened` and `idiomatic_slow_path` over `literal` unless literal structure is itself what needs review.

## Generated File Rules

- Include `compliance/generated/test_contract.h`.
- Provide a single `main(void)` entry point.
- Keep the file self-contained.
- Add a short comment near the top naming the upstream JS source and the semantic intent.
- Use clear local variable names over prompt-shaped names.

## Result Policy

- `PASS` means the translated semantic check succeeded.
- `KNOWN_UNSUPPORTED` means the test is intentionally deferred.
- `FAIL` means the translated test compiled and ran, but the observed result was wrong.

When the slow-path contract is already clear, prefer a passing `idiomatic_slow_path` fixture to a boundary-only `KNOWN_UNSUPPORTED` placeholder.
