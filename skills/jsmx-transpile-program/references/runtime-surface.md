# Runtime Surface

Use this reference to decide whether a JS entrypoint can become a production C program today.

## Outcome Classes

- `production_flattened`
  - the program can be expressed directly through current `jsmx` plus ordinary C control flow
- `production_slow_path`
  - the program needs explicit translator-owned host or semantic glue, but the contract is already clear
- `manual_runtime_needed`
  - the program depends on runtime or host behavior that the current repo does not define well enough to emit honest production C

## Direct `jsmx` Coverage

Treat these as direct-lowerable when the entrypoint stays inside the current flattened boundary:

- value kinds:
  - `undefined`, `null`, booleans, numbers, strings
  - native and JSON-backed objects and arrays
  - dense arrays only
  - feature-gated regex values and match iterators
- value operations:
  - truthiness
  - strict and abstract equality
  - arithmetic, integer coercion, bitwise ops, shifts
  - relational comparison
  - `typeof`
  - nullish detection and explicit `??` / ternary lowering patterns
- string operations through `jsmethod` / `jsval`:
  - concat, trim, repeat, padding
  - `indexOf`, `lastIndexOf`, `includes`, `startsWith`, `endsWith`
  - `charAt`, `at`, `charCodeAt`, `codePointAt`
  - `slice`, `substring`, `substr`
  - `normalize`, `toWellFormed`
  - feature-gated regex-backed string methods
- object and array operations:
  - own-property get/set/delete
  - ordered key/value reads
  - own-property copy and clone
  - dense-array push/pop/shift/unshift/clone/splice
  - dense spread-style assembly already modeled in the corpus
- regex:
  - direct-lowered native regex values
  - translator-owned `/u` rewrite families already documented in the regex guidance

## Translator-Owned Syntax Lowering

These are not runtime features. They are the translator's job and are acceptable in production output when the JS semantics remain ordinary:

- top-level statements lowered into `main(...)`
- local variables and assignments
- `if`, `switch`, `for`, `while`, and `do` loops
- `break`, `continue`, and `return`
- helper functions lowered into `static` C functions when they do not require:
  - closure capture
  - dynamic `this`
  - `arguments`
  - function objects as first-class values
  - `eval`

If the JS function behavior depends on those missing categories, classify the program as `manual_runtime_needed`.

## Clear Slow-Path Cases

Use `production_slow_path` only when the contract is already clear. The main allowed cases are:

- host glue outside `jsmx`:
  - CLI argument decoding
  - stdin/stdout/stderr bridges
  - exit-code handling
  - environment and working-directory access through libc
  - synchronous UTF-8 text file reads and writes through libc
- known semantic glue already established in the repo:
  - explicit receiver/argument coercion around string methods through `jsmethod`
  - repo-documented regex constructor choice and rewrite selection
  - current idiomatic slow-path patterns already represented in the committed corpus

If the repo does not already make the slow-path contract clear, stop instead of inventing one.

## Manual-Runtime-Needed Cases

Classify the program as `manual_runtime_needed` when it depends on behavior like:

- modules or loaders whose semantics are not being inlined explicitly
  - dynamic `require(...)`
  - ESM loader semantics
- async or event-loop behavior:
  - promises
  - timers
  - streams with async callbacks
  - networking
- unsupported JS categories:
  - symbols
  - bigint
  - prototype-chain-sensitive behavior
  - descriptors or accessors
  - sparse arrays or holes
  - functions as values with closure semantics
- host contracts with no clear current mapping:
  - exact `console.log` object formatting like Node `util.inspect`
  - arbitrary core-module behavior beyond the documented host bridges

When in doubt, stop with a boundary report rather than silently approximating the missing runtime.
