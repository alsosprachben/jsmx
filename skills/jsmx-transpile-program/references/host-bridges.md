# Host Bridges

This skill targets Node-invoked scripts, but it should bridge host behavior explicitly through C and libc instead of pretending a full Node runtime exists.

## General Rule

- Prefer direct host bridges over synthetic global-object emulation.
- Build JS values only when the program actually needs JS-value behavior.
- If a host surface is not covered below and cannot be mapped honestly, classify it as `manual_runtime_needed`.
- When a host bridge is reusable across multiple transpiled programs, place it
  under `runtime_modules/` rather than inside `example/` or a one-off program
  directory.
- Use shared implementations plus runtime profiles:
  - `runtime_modules/shared/` for reusable host-backed C implementations
  - `runtime_modules/node/` and `runtime_modules/wintertc/` for runtime-facing
    wrappers and profile manifests

## `process.argv`

When the JS program uses `process.argv`, generate a Node-shaped argument array explicitly:

- `process.argv[0]`
  - executable path from C `argv[0]`
- `process.argv[1]`
  - baked-in original JS entrypoint path
- remaining entries
  - C `argv[1..]`

Use the narrowest representation that preserves semantics:

- if the script only indexes or compares CLI strings, direct C `argv` access is acceptable
- if the script treats `process.argv` as a JS value, build a native dense array of strings with `jsval_array_new(...)` and `jsval_array_push(...)`

## Standard I/O

Map I/O directly to libc:

- stdin
  - use an explicit helper that reads all text input when the JS program consumes stdin as a whole string
- stdout / stderr
  - use `fwrite`, `fprintf`, or `printf` directly
- exit status
  - return from `main(...)` or call `exit(...)` explicitly

If the JS program depends on streaming async semantics, stop and mark it `manual_runtime_needed`.

## `console.log` And `console.error`

Support only cases with a clear printable contract:

- strings already known to be strings
- primitive values with direct, explicit formatting
- JSON-compatible objects or arrays only when JSON text is what the program actually wants

Do not claim exact Node `console` parity for object inspection formatting. If the program depends on Node-style `util.inspect` output, mark it `manual_runtime_needed`.

## Environment And Working Directory

These are allowed as explicit slow-path host bridges:

- `process.env.NAME`
  - `getenv("NAME")`
- `process.cwd()`
  - `getcwd(...)`

Wrap the result as a JS string value only when later JS-value semantics require it.

If the program mutates `process.env` or depends on finer Node process semantics, stop.

## Synchronous Text Files

These are allowed as explicit slow-path host bridges when the JS program is synchronous and text-oriented:

- a static top-level `const fs = require("fs")` or
  `const fs = require("node:fs")` binding is acceptable translator syntax
  sugar for this narrow bridge; lower it directly to libc file I/O without
  modeling the Node module loader
- a static local helper module that wraps only the blessed synchronous `fs`
  subset is also acceptable when the transpiled C inlines or directly includes
  that helper logic rather than modeling generic CommonJS loading
- reusable wrappers for this subset should live under `runtime_modules/node/`
  and point at shared host-backed C support under `runtime_modules/shared/`
- `fs.readFileSync(path, "utf8")`
  - `fopen` + `fread` into an explicit UTF-8 buffer
- `fs.readdirSync(path)`
  - `opendir` + `readdir`
- `fs.lstatSync(path)`
  - `lstat(...)`
- `fs.statSync(path)`
  - `stat(...)`
- `fs.realpathSync(path)`
  - `realpath(...)`
- `fs.writeFileSync(path, text, "utf8")`
  - `fopen` + `fwrite`

Keep the encoding assumption explicit in comments. Binary buffers, streams,
file descriptors, and async filesystem flows are out of scope for this skill.

## WinterTC Crypto

The current reusable WinterTC crypto bridge is sync-only:

- `runtime_modules/shared/webcrypto_openssl.h`
  - random bytes
  - UUID v4 assembly
- `runtime_modules/wintertc/profile.json`
  - `globalThis.crypto`
  - `crypto.randomUUID()`
  - `crypto.getRandomValues(typedArray)`
  - stable `crypto.subtle` object identity

Do not claim async `SubtleCrypto` method coverage until the runtime has real
Promise support.

## Out Of Scope Host Surfaces

Treat these as `manual_runtime_needed` unless the user explicitly asks for a larger runtime design:

- arbitrary core modules
- networking
- timers
- worker threads
- child processes
- async streams
- exact TTY behavior
- Node module loader semantics
