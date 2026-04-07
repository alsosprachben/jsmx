# Lowering Recipes

Use these recipes when converting ordinary JS program structure into a standalone C entrypoint.

## Statements And Locals

- lower top-level JS statements into `main(...)`
- lower helper functions into `static` C functions when they do not need closure semantics
- use `jsval_t` for values that need JS-visible coercion or identity behavior
- use native C scalars only when preserving semantics is obvious and reviewable
  - loop counters
  - fixed indices
  - already-decoded exit codes

## Values

Prefer the current `jsval` helper surface directly:

- truthiness:
  - `jsval_truthy(...)`
- `typeof`:
  - `jsval_typeof(...)`
- nullish coalescing:
  - evaluate the left operand once
  - branch on `jsval_is_nullish(left)`
- ternary:
  - evaluate the condition once
  - branch on `jsval_truthy(cond)`
- arithmetic and comparison:
  - use the existing numeric, equality, relational, bitwise, and shift helpers

## Strings

Use `jsmethod` or the `jsval_method_string_*` bridge when JS coercion matters.

Examples:

- concat:
  - `jsval_method_string_concat(...)`
- slicing:
  - `jsval_method_string_slice(...)`
  - `jsval_method_string_substring(...)`
  - `jsval_method_string_substr(...)`
- search and membership:
  - `jsval_method_string_index_of(...)`
  - `jsval_method_string_includes(...)`
  - `jsval_method_string_starts_with(...)`
  - `jsval_method_string_ends_with(...)`

Keep measure-then-execute APIs explicit when the generated program needs caller-managed output buffers.

## Objects And Arrays

Prefer the direct helper surface already established in the corpus:

- objects:
  - `jsval_object_get_utf8(...)`
  - `jsval_object_set_utf8(...)`
  - `jsval_object_copy_own(...)`
  - `jsval_object_clone_own(...)`
  - `jsval_object_key_at(...)`
  - `jsval_object_value_at(...)`
- arrays:
  - `jsval_array_get(...)`
  - `jsval_array_set(...)`
  - `jsval_array_push(...)`
  - `jsval_array_pop(...)`
  - `jsval_array_shift(...)`
  - `jsval_array_unshift(...)`
  - `jsval_array_clone_dense(...)`
  - `jsval_array_splice_dense(...)`

Remember the current boundary:

- arrays are dense and capacity-bounded
- sparse arrays and holes are still out of scope
- JSON-backed values are readable without implicit mutation
- promotion remains explicit when mutation begins

## Regex

When the program uses direct-lowered regex values:

- use `jsval_regexp_new(...)` for short-lived or one-shot regexes
- use `jsval_regexp_new_jit(...)` only when the regex is clearly hoisted and reused
- keep rewrite-backed `/u` helper families explicit when the pattern matches the repo's documented rewrite lanes
- for UTF-8-oriented production tools that only need low-level compile / exec /
  search, prefer `jsregex8_*` over a UTF-16 transcode loop

Do not emit backend-specific `pcre2_*` calls directly in the generated program.

## Output And Error Handling

- keep exit behavior explicit
- propagate host failures through readable stderr messages
- prefer one clear failure path per helper call
- when the source JS throws semantics that the current runtime cannot model honestly, stop and mark the program `manual_runtime_needed` instead of faking exception behavior
