# Compliance Corpus

This directory is the stable landing zone for real JavaScript compliance tests and their generated `jsmx`-targeted C fixtures. The current committed suites cover string built-ins, including normalization, `concat`, `replace`, `replaceAll`, translator-facing callback replacers for `replace` / `replaceAll`, case conversion, well-formedness, trim methods (`trim`, `trimStart`, `trimEnd`) plus the legacy behavioral aliases (`trimLeft`, `trimRight`), `repeat`, padding (`padStart`, `padEnd`), the non-regex search core (`indexOf`, `lastIndexOf`, `includes`, `startsWith`, `endsWith`) plus `split`, feature-gated regex-backed `search`, `replace`, `replaceAll`, `split`, `exec`, `match`, and `matchAll`, accessor methods (`charAt`, `at`, `charCodeAt`, `codePointAt`), and substring extraction (`slice`, `substring`, `substr`); a feature-gated `regex` suite covering semantic `RegExp.prototype.source`, `RegExp.prototype.test`, `RegExp.prototype[Symbol.matchAll]`, supported regex flag/state access, constructor-state behavior, and deeper repeated `exec` / `test` / `match` state fidelity; official and repo-authored `jsval` value semantics including test262 strict-equality and primitive abstract equality/inequality, logical-operator slices (`!`, `&&`, `||`), primitive numeric coercion/arithmetic slices, primitive integer coercion plus bitwise and shift slices (`~`, `&`, `|`, `^`, `<<`, `>>`, `>>>`), and primitive relational-operator slices (`<`, `<=`, `>`, `>=`) across both numeric and string ordering paths, explicit object/array promotion flows, shallow capacity-planned promotion of selected parsed subtrees, and native object/array helpers such as own-property checks, ordered key/value access, delete, overwrite, push, pop, shift, unshift, and observable dense length writes. Regex compatibility choices are translator-owned first; use the repo-local regex compatibility guide when deciding direct lowering, rewrite-backed lowering, idiomatic slow path, or unsupported classification, including the approved `/u` rewrite families for lone-surrogate atoms, fixed no-capture literal sequences, and simple no-capture literal-member and negated literal-member classes.

Layout:

- `js/`: source JS tests, organized by suite
- `generated/`: committed C fixtures translated from those JS tests
- `manifest.json`: source-to-generated mapping, expected outcome metadata, and runner contract

The repository does not regenerate these fixtures in `make`. Translation is an authoring workflow; committed outputs are the build and review artifacts. The committed fixtures are compiled and executed by the manifest-driven `make test_compliance` path. The runner now builds one shared runtime archive per invocation, then fans fixture compiles and executions out in parallel while still reporting results in manifest order; set `JOBS=N` or `COMPLIANCE_JOBS=N` to override the default worker count. Manifest entries may be expected to `pass` or remain `known_unsupported` when the upstream JS surface still exceeds the current `jsmx` API. The runner respects `CFLAGS`, `LDFLAGS`, and `LDLIBS`, so the same corpus can be checked against both the default numeric backend and the `USE_LIBC` matrix, and feature-gated cases can declare `required_features` such as `regex`.

For official coverage, prefer a real upstream source file plus one idiomatic generated translation. When an official file mixes flattenable checks with already-understood boundary behavior, prefer one passing idiomatic slow-path translation over omitting those checks. Repo-authored sources should be reserved for `jsmx`-specific contracts that the upstream suite does not express well, such as JSON-backed parity or planned-capacity behavior. That also applies when an upstream directory only contains built-in metadata or same-function-object alias checks, as with Annex B `trimLeft` / `trimRight`; in that case, cover the runtime behavior with repo-authored parity fixtures instead of pretending the metadata file is the semantic target.

For new or updated regex-bearing entries, make the manifest `notes` begin with
one of `Direct-lowered:`, `Rewrite-backed:`, `Idiomatic slow path:`, or
`Unsupported:` so the translator choice is explicit in the corpus itself.

Each manifest case also carries a `lowering_class`:

- `static_pass`: the behavior is flattenable through current `jsmx` / `jsmethod` APIs
- `slow_path_needed`: the behavior is valid JS but needs a translator-emitted dynamic slow path outside current `jsmx`
- `unsupported`: the behavior is intentionally outside the current project scope

Each case also carries a `translation_mode`:

- `idiomatic_flattened`: the fixture demonstrates the intended flattened lowering, not a literal restaging of the JS source
- `idiomatic_slow_path`: the fixture demonstrates the intended translator-emitted slow path outside `jsmx`
- `literal`: the fixture intentionally mirrors the upstream source shape more directly

That classification is descriptive. `expected_status` remains the executable contract for the current runner, so a `slow_path_needed` case may still pass when the fixture demonstrates the intended slow path explicitly.

The repo-local skill at `skills/jsmx-transpile-tests/` and the boundary doc at `docs/flattening-boundary.md` define the preferred translation workflow and output contract.
