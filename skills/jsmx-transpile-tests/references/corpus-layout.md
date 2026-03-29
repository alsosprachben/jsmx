# Corpus Layout

The compliance corpus is repository data, not an interactive prompt artifact.

## Directories

- `compliance/js/<suite>/`
  - vendored or copied JS source tests
- `compliance/generated/<suite>/`
  - committed generated C fixtures derived from those JS tests
- `compliance/generated/test_contract.h`
  - shared reporting contract for generated fixtures
- `compliance/manifest.json`
  - source-to-generated mapping and expected outcome metadata

## Naming

- Keep suite names stable and lowercase, for example `strings`.
- Name generated fixtures after the source stem when possible.
- Prefer deterministic paths like:
  - `compliance/js/strings/test262-normalize-nfc.js`
  - `compliance/generated/strings/test262-normalize-nfc.c`

## Manifest Fields

Each case in `compliance/manifest.json` should include:

- `id`: stable identifier
- `suite`: logical suite name
- `source`: repo-relative JS source path
- `generated`: repo-relative generated C path
- `lowering_class`: one of `static_pass`, `slow_path_needed`, `unsupported`
- `expected_status`: one of `pass`, `known_unsupported`
- `notes`: short explanation when useful

## Generated Fixture Contract

Generated fixtures should include `compliance/generated/test_contract.h` and follow its exit codes:

- `0`: pass
- `1`: wrong result or execution failure
- `2`: known unsupported

The fixture should print a single status line that includes the suite and case name.

## Review Expectations

- Generated C is committed and reviewed like handwritten code.
- The manifest is part of the reviewed change.
- Build systems and CI should compile committed outputs only; they should not regenerate fixtures with a model call.
