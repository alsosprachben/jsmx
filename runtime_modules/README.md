# Runtime Modules

`runtime_modules/` is the reusable host-module area for transpiled production
programs.

It is intentionally separate from:

- core `jsmx` language/runtime helpers
- `example/` programs that consume the module layer

The layout is:

- `shared/`
  - reusable host-backed C implementations that are not runtime-exclusive
- `node/`
  - Node-facing JS wrappers and the Node runtime profile manifest
- `wintertc/`
  - WinterTC-facing runtime profile manifest and future runtime-specific
    wrappers

This area is translator-owned support code. It does not model a general module
loader.

- shared implementations should exist once when multiple runtime profiles can
  reuse them
- runtime profiles should list which modules are exposed in a given
  environment
- examples and generated programs should include or import these modules
  directly rather than copying host bridges into `example/`

The first shared module is the synchronous text/filesystem subset used by the
grep demo:

- shared C bridge:
  - `runtime_modules/shared/fs_sync.h`
- Node wrapper:
  - `runtime_modules/node/fs_sync.js`

The next shared module is the WinterTC-oriented sync crypto bridge:

- shared C bridge:
  - `runtime_modules/shared/webcrypto_openssl.h`
- WinterTC profile exposure:
  - `runtime_modules/wintertc/profile.json`

The FaaS handler bridge glues the Fetch JS object model to an HTTP/1.1
embedder. It defines the `faas_fetch_handler_fn` C signature every
hosted function (hand-written or transpiled from
`export default { async fetch(request) { ... } }`) must implement,
plus a drain-until-settled pump and a Response serializer:

- shared C bridge:
  - `runtime_modules/shared/faas_bridge.h`
- reference usage and test coverage:
  - `test_faas.c` at the repo root
