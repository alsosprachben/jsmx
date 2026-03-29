---
name: jsmx-transpile-tests
description: Translate real JavaScript compliance tests into self-contained C fixtures that target jsmx. Use when converting JS test-suite files into committed C tests under compliance/generated, with explicit jsmx APIs, explicit lowering classification, and no hidden promotion or allocation semantics.
---

# JSMX Transpile Tests

Use this skill when a JavaScript compliance test should become a committed C fixture for this repository.

Read these references before generating or editing fixtures:

- `references/style.md`: translation rules, API preferences, unsupported-policy
- `references/corpus-layout.md`: target directories, manifest fields, naming, and test contract

## Workflow

1. Read the source JS test and identify the smallest semantic unit worth translating.
2. Choose the narrowest `jsmx` surface that matches the test:
   - `jsstr` for pure string semantics
   - `jsval` for values, JSON-backed/native transitions, and object/array behavior
3. Generate one self-contained C file under `compliance/generated/<suite>/`.
4. Make the generated file include `compliance/generated/test_contract.h` and report one of:
   - `PASS`
   - `KNOWN_UNSUPPORTED`
   - `FAIL`
5. Add or update the corresponding `compliance/manifest.json` entry, including its `lowering_class` and `translation_mode`.
6. Build and run the generated fixture locally if possible.

## Rules

- Do not invent semantics to force a pass.
- Prefer explicit `KNOWN_UNSUPPORTED` to partial or fake implementations.
- Set `lowering_class` using `docs/flattening-boundary.md`:
  - `static_pass`
  - `slow_path_needed`
  - `unsupported`
- Set `translation_mode` to show how the fixture was translated:
  - `idiomatic_flattened`
  - `idiomatic_slow_path`
  - `literal`
- Keep promotion explicit with `jsval_region_promote_root()` or `jsval_promote_in_place()`.
- Keep all storage caller-owned; do not introduce heap allocation patterns foreign to `jsmx`.
- Commit generated C outputs and manifest updates. Do not require live model calls in `make` or CI.

## Output Shape

- One upstream JS source file maps to one generated C fixture.
- Generated files should be readable and reviewable, not minified or prompt-shaped.
- Preserve the original test intent in comments near the top of the generated C file.
