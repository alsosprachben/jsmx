# Regex Compatibility

This document defines how the translator should choose regex lowerings for the
current PCRE2-backed `jsmx` runtime.

## Ownership

Regex compatibility is **translator-owned first**.

The runtime should execute a chosen lowering efficiently. It should not be the
primary place that decides whether a JS regex is direct, rewritten, slow-path,
or unsupported. The emitted C should already embody that choice.

## Lowering Choices

Every regex-bearing fixture or emitted lowering should be classified as one of:

- `direct`
  - lower the original JS pattern and flags straight to
    `jsval_regexp_new(...)`
  - use this when the current runtime already models the JS behavior honestly
- `rewrite`
  - rewrite the JS pattern and/or flags before lowering to
    `jsval_regexp_new(...)`
  - use this only when the repo already documents a reviewed rewrite recipe
  - the generated fixture comment and manifest `notes` must say it is
    rewrite-backed and name the compatibility reason
- `slow_path`
  - keep the behavior as valid JS, but implement it outside current `jsmx`
    runtime calls
  - use this when the behavior is understood but exceeds the current flattened
    runtime boundary
- `unsupported`
  - do not pretend the runtime can execute it today
  - use this when the behavior is intentionally out of scope or no honest
    lowering exists yet

## Current Direct-Lowered Surface

Use direct lowering for the current supported subset:

- flags limited to `g`, `i`, `m`, `s`, `u`, `y`
- no named groups
- no `d` or `v`
- no Unicode sets
- no exact `RegExp.prototype.source` serialization claims beyond the current
  semantic `source`
- result arrays where `groups` remains `undefined`

If a test depends only on those semantics, lower it directly and say so in the
manifest `notes` using a `Direct-lowered:` prefix.

## Current Rewrite Policy

Do **not** invent ad hoc regex rewrites while translating fixtures.

Only use a rewrite when all of these are true:

- the incompatibility is explicitly documented in this repo
- the rewrite rule is specific and reviewable
- the rewritten lowering preserves the JS-visible behavior the fixture is
  claiming to prove
- the generated fixture comment and manifest `notes` both identify the case as
  `Rewrite-backed: ...`

Current status:

- the single-literal lone-surrogate `/u` no-capture family now has a reviewed
  rewrite-backed recipe covering `exec`, `test`, `search`, `match`,
  `replace`, `replaceAll`, callback replacers, and `split`
- broader Unicode/surrogate-sensitive `/u` behavior remains outside direct
  lowering until additional rewrite recipes are reviewed

## Current Known Gaps

| Family | Representative case | Current classification | Next action |
| --- | --- | --- | --- |
| Single literal lone-surrogate `/u` no-capture exec/test/search/match/matchAll/replace/replaceAll/split behavior | `strings/test262/exec/u-lastindex-adv` and `strings/jsmx/u-literal-surrogate-matchAll-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to literal no-capture string-method, iterator, and callback-replacer cases until more recipes are reviewed |
| Broader Unicode/surrogate-sensitive `/u` behavior | future cases beyond one literal lone-surrogate atom | `Unsupported:` rewrite-candidate | Land additional reviewed rewrite recipes before translating them as passing cases |
| Reflective regex property-override behavior | `strings/test262/matchAll/flags-nonglobal-throws` | `Idiomatic slow path:` | Keep it as an explicit slow path unless the runtime grows a reflective regex object model |
| `d` / `v` flag surface | `regex/test262/flags/this-val-regexp` | `Unsupported:` beyond the current `gimsuy` subset | Add runtime/backend support before expanding coverage |
| Named groups and `groups` objects | future `exec` / `match` named-group files | `Unsupported:` | Expand the semantic regex result surface before translating those files as passing cases |

## Approved Rewrite: Single Literal Lone Surrogate Under `/u`

Use a rewrite-backed lowering only when all of these are true:

- the JS pattern is exactly one literal surrogate code unit under the `u` flag
  (`\uD800..\uDFFF`)
- the translated case only needs literal no-capture
  `exec`/`test`/`search`/`match`/`replace`/`replaceAll`/`split` semantics
- there are no captures, character classes, alternation, quantifiers,
  backreferences, or other regex operators in the pattern

Rewrite recipe:

- do **not** lower the pattern through `jsval_regexp_new(...)`
- emit explicit UTF-16 matching logic using:
  - `jsregex_exec_u_literal_surrogate_utf16(...)` for exec-style result needs
  - `jsregex_test_u_literal_surrogate_utf16(...)` for test-style booleanization
  - `jsregex_search_u_literal_surrogate_utf16(...)` for search-style index needs
- for string-method surfaces, use the dedicated translator-facing `jsval`
  helpers:
  - `jsval_method_string_match_all_u_literal_surrogate(...)`
  - `jsval_method_string_match_u_literal_surrogate(...)`
  - `jsval_method_string_replace_u_literal_surrogate(...)`
  - `jsval_method_string_replace_all_u_literal_surrogate(...)`
  - `jsval_method_string_replace_u_literal_surrogate_fn(...)`
  - `jsval_method_string_replace_all_u_literal_surrogate_fn(...)`
  - `jsval_method_string_split_u_literal_surrogate(...)`
- for a lone high surrogate atom, match only isolated high surrogates that are
  not followed by a low surrogate
- for a lone low surrogate atom, match only isolated low surrogates that are
  not preceded by a high surrogate
- scan the subject with Unicode code-point advancement semantics so surrogate
  pairs are skipped as indivisible code points during the search
- for `matchAll`, lower to the dedicated iterator helper and keep the same
  no-capture result shape already used by the semantic regex iterator:
  `[match]`, `length`, `index`, `input`, and `groups = undefined`

This rewrite is intentionally narrow. Any broader `/u` surrogate-sensitive case
still needs a separate reviewed recipe before it can be translated as passing.

## Current Slow-Path Policy

Use an idiomatic slow path when the JS behavior is understood but sits above the
current regex runtime boundary.

Common examples:

- reflective property overrides on regex values
- callback- or accessor-driven coercion around regex methods
- non-native regex-like object behavior that the current runtime does not model
  directly

Manifest `notes` for those cases should begin with `Idiomatic slow path:`.

## Current Unsupported Policy

Keep these out of passing runtime coverage unless the runtime surface changes:

- named capture groups and `groups` objects
- named replacement tokens
- `d` / match indices
- `v` / Unicode sets
- exact `source` serialization edge behavior

If a valid JS regex case depends on one of those and no reviewed slow path is
available, classify it as unsupported rather than approximating it.

Manifest `notes` for those cases should begin with `Unsupported:`.

## Manifest Notes Convention

For new or updated regex-bearing entries, start `notes` with one of:

- `Direct-lowered:`
- `Rewrite-backed:`
- `Idiomatic slow path:`
- `Unsupported:`

Use the rest of the note to name the runtime entrypoint or compatibility reason
so the translator decision is reviewable from the corpus alone.
