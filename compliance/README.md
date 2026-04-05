# Compliance Corpus

This directory is the stable landing zone for real JavaScript compliance tests
and their generated `jsmx`-targeted C fixtures.

## Current Coverage

The committed suites currently cover:

- string built-ins
  - normalization
  - `concat`
  - `replace`
  - `replaceAll`
  - translator-facing callback replacers for `replace` / `replaceAll`
  - case conversion
  - well-formedness
  - trim methods:
    - `trim`
    - `trimStart`
    - `trimEnd`
    - legacy aliases:
      - `trimLeft`
      - `trimRight`
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
  - supported regex flag / state access
  - constructor-state behavior
  - deeper repeated `exec` / `test` / `match` state fidelity
- official and repo-authored `jsval` value semantics
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
  - explicit object / array promotion flows
  - shallow capacity-planned promotion of selected parsed subtrees
  - native object / array helpers such as:
    - own-property checks
    - ordered key / value access
    - shallow own-property copy
    - fresh own-property clone into a new native object
    - fresh dense-array clone into a new native array
    - delete
    - overwrite
    - push
    - pop
    - shift
    - unshift
    - observable dense length writes

Regex compatibility choices are translator-owned first.

- Use the repo-local regex compatibility guide when deciding:
  - direct lowering
  - rewrite-backed lowering
  - idiomatic slow path
  - unsupported classification
- That includes the approved `/u` rewrite families for:
  - lone-surrogate atoms
  - fixed no-capture literal sequences
  - simple no-capture literal-member classes
  - simple no-capture negated literal-member classes

## Layout

- `js/`
  - source JS tests, organized by suite
- `generated/`
  - committed C fixtures translated from those JS tests
- `manifest.json`
  - source-to-generated mapping
  - expected-outcome metadata
  - runner contract

## Workflow

The repository does not regenerate these fixtures in `make`.

- Translation is an authoring workflow.
- Committed outputs are the build and review artifacts.
- The committed fixtures are compiled and executed by the manifest-driven
  `make test_compliance` path.

The runner:

- builds one shared runtime archive per invocation
- fans fixture compiles and executions out in parallel
- still reports results in manifest order
- accepts `JOBS=N` or `COMPLIANCE_JOBS=N` to override worker count
- respects:
  - `CFLAGS`
  - `LDFLAGS`
  - `LDLIBS`

That means the same corpus can be checked against:

- the default numeric backend
- the `USE_LIBC` matrix
- feature-gated cases that declare `required_features` such as `regex`

Manifest entries may be expected to:

- `pass`
- remain `known_unsupported` when the upstream JS surface still exceeds the
  current `jsmx` API

## Translation Guidance

For official coverage:

- prefer a real upstream source file plus one idiomatic generated translation
- when an official file mixes flattenable checks with already-understood
  boundary behavior, prefer one passing idiomatic slow-path translation over
  omitting those checks

Repo-authored sources should be reserved for `jsmx`-specific contracts that the
upstream suite does not express well, such as:

- JSON-backed parity
- planned-capacity behavior

That also applies when an upstream directory only contains:

- built-in metadata checks
- same-function-object alias checks

Example:

- Annex B `trimLeft` / `trimRight`
  - cover the runtime behavior with repo-authored parity fixtures
  - do not pretend the metadata file is the semantic target

For new or updated regex-bearing entries, make the manifest `notes` begin with
one of:

- `Direct-lowered:`
- `Rewrite-backed:`
- `Idiomatic slow path:`
- `Unsupported:`

That keeps the translator choice explicit in the corpus itself.

## Manifest Metadata

Each manifest case carries a `lowering_class`:

- `static_pass`
  - the behavior is flattenable through current `jsmx` / `jsmethod` APIs
- `slow_path_needed`
  - the behavior is valid JS but needs a translator-emitted dynamic slow path
    outside current `jsmx`
- `unsupported`
  - the behavior is intentionally outside the current project scope

Each case also carries a `translation_mode`:

- `idiomatic_flattened`
  - the fixture demonstrates the intended flattened lowering
  - not a literal restaging of the JS source
- `idiomatic_slow_path`
  - the fixture demonstrates the intended translator-emitted slow path outside
    `jsmx`
- `literal`
  - the fixture intentionally mirrors the upstream source shape more directly

That classification is descriptive.

- `expected_status` remains the executable contract for the current runner
- a `slow_path_needed` case may still pass when the fixture demonstrates the
  intended slow path explicitly

## Related Docs

- `skills/jsmx-transpile-tests/`
  - repo-local translation skill and workflow guidance
- `docs/flattening-boundary.md`
  - preferred translation workflow and flattened-output contract
