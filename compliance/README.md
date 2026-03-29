# Compliance Corpus

This directory is the stable landing zone for real JavaScript compliance tests and their generated `jsmx`-targeted C fixtures.

Layout:

- `js/`: source JS tests, organized by suite
- `generated/`: committed C fixtures translated from those JS tests
- `manifest.json`: source-to-generated mapping, expected outcome metadata, and runner contract

The repository does not regenerate these fixtures in `make`. Translation is an authoring workflow; committed outputs are the build and review artifacts. The committed fixtures are compiled and executed by the manifest-driven `make test_compliance` path. Manifest entries may be expected to `pass` or remain `known_unsupported` when the upstream JS surface still exceeds the current `jsmx` API.

The repo-local skill at `skills/jsmx-transpile-tests/` defines the preferred translation workflow and output contract.
