# Host Bridges

This skill targets Node-invoked scripts, but it should bridge host behavior explicitly through C and libc instead of pretending a full Node runtime exists.

## General Rule

- Prefer direct host bridges over synthetic global-object emulation.
- Build JS values only when the program actually needs JS-value behavior.
- If a host surface is not covered below and cannot be mapped honestly, classify it as `manual_runtime_needed`.

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

- `fs.readFileSync(path, "utf8")`
  - `fopen` + `fread` into an explicit UTF-8 buffer
- `fs.writeFileSync(path, text, "utf8")`
  - `fopen` + `fwrite`

Keep the encoding assumption explicit in comments. Binary buffers, streams, file descriptors, and async filesystem flows are out of scope for this skill.

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
