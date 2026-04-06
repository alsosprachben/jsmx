---
name: jsmx-transpile-program
description: Convert a JavaScript entrypoint into a standalone C program built against jsmx. Use when translating a JS or Node-style script into a reviewable production binary with explicit host mapping, direct jsmx lowering where possible, and honest manual-runtime-needed stops where the current runtime boundary is exceeded.
---

# JSMX Transpile Program

Use this skill when a JavaScript file should become a standalone C program, not a compliance fixture.

Read these references before generating or editing output:

- `references/runtime-surface.md`: direct `jsmx` coverage, slow-path policy, and stop conditions
- `references/program-shape.md`: required generated-program structure and compile commands
- `references/host-bridges.md`: supported Node-to-C host mappings
- `references/lowering-recipes.md`: translator-owned lowering patterns for statements, values, strings, objects, arrays, and regexes

## Workflow

1. Inspect the JS entrypoint and classify it as:
   - `production_flattened`
   - `production_slow_path`
   - `manual_runtime_needed`
2. Inventory the program's:
   - value and string semantics
   - object and array behavior
   - regex behavior
   - host usage (`process`, console, stdin/stdout, filesystem, env, modules)
3. Choose the narrowest `jsmx` surface that matches the program:
   - `jsval` for JS values, arrays, objects, and control-flow-visible semantics
   - `jsmethod` for JS string built-ins with coercion
   - `jsregex` / `jsval` regex entrypoints for direct-lowered regexes
   - `runtime_modules/` for reusable host-module bridges that should not be
     embedded ad hoc inside an example or one-off generated program
4. Generate one reviewable C file with `main(int argc, char **argv)` unless the user explicitly requests a larger scaffold.
5. Include one explicit compile command and note any required feature flags such as regex support.
6. Validate the generated binary against Node when the entrypoint stays inside the supported host/runtime contract.

## Rules

- Do not route production programs through `compliance/generated/test_contract.h` or the compliance manifest.
- Keep direct lowering, translator-owned slow path, and manual-runtime-needed outcomes explicit in comments near the top of the generated C file.
- Prefer direct `jsmx` lowering where available.
- Use translator-owned slow paths only when the contract is already clear from the current runtime and repo guidance.
- Do not invent Node or browser runtime semantics that the generated program cannot actually provide.
- Keep ownership explicit:
  - caller-owned region buffers
  - caller-owned I/O buffers
  - explicit cleanup for heap-backed buffers if the generated program needs them
- Keep regex constructor choice explicit and reviewable:
  - `jsval_regexp_new(...)` for one-shot or short-lived regexes
  - `jsval_regexp_new_jit(...)` only for clearly hoisted, reused regexes

## Output Shape

- One JS entrypoint maps to one standalone C source file by default.
- The generated source should be reviewable handwritten-style C, not fixture-shaped or prompt-shaped output.
- Put a short comment block near the top covering:
  - source JS file
  - intended runtime behavior
  - classification (`production_flattened`, `production_slow_path`, or `manual_runtime_needed`)
  - any host/runtime gaps that remain
- If the program cannot be lowered honestly, stop with a precise boundary report instead of emitting fake production C.
