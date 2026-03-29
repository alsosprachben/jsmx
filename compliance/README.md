# Compliance Corpus

This directory is the stable landing zone for real JavaScript compliance tests and their generated `jsmx`-targeted C fixtures. The current committed suites cover string built-ins, core `jsval` value semantics, explicit object/array promotion flows, and native object/array helpers such as own-property checks, delete, push, and dense length writes.

Layout:

- `js/`: source JS tests, organized by suite
- `generated/`: committed C fixtures translated from those JS tests
- `manifest.json`: source-to-generated mapping, expected outcome metadata, and runner contract

The repository does not regenerate these fixtures in `make`. Translation is an authoring workflow; committed outputs are the build and review artifacts. The committed fixtures are compiled and executed by the manifest-driven `make test_compliance` path. Manifest entries may be expected to `pass` or remain `known_unsupported` when the upstream JS surface still exceeds the current `jsmx` API.

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
