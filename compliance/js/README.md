# Source JS Tests

Store vendored or copied JavaScript compliance tests here, grouped by suite.

Use vendored upstream sources when the JS behavior maps cleanly onto the current flattened layer. Use small repo-authored `jsmx/` sources when the fixture needs to demonstrate an intended lowering pattern such as explicit JSON-backed promotion.

Keep the original file names when practical so source provenance remains easy to audit against the generated C fixtures and manifest entries.

When translating a source test into C, follow `skills/jsmx-transpile-tests/` and record the mapping in `compliance/manifest.json`.
