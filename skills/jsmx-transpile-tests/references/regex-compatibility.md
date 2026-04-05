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
  - for longer-lived direct-lowered regexes that will be reused across many
    matches, the translator may choose `jsval_regexp_new_jit(...)` instead
    of `jsval_regexp_new(...)`
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
- named groups and `groups` objects on direct-lowered `exec`,
  non-global `match`, `matchAll`, `replace`, and `replaceAll`
- direct-lowered named replacement tokens (`$<name>`) and replace-callback
  `groups` arguments
- no `d` or `v`
- no Unicode sets
- no exact `RegExp.prototype.source` serialization claims beyond the current
  semantic `source`
- exec-shaped results where `groups` is a native object for named-group regexes
  and `undefined` otherwise

If a test depends only on those semantics, lower it directly and say so in the
manifest `notes` using a `Direct-lowered:` prefix.

JIT choice is not a semantic classification change. It is a translator-owned
construction choice within the direct-lowered lane:

- use `jsval_regexp_new(...)` for short-lived or compile-per-use regexes
- use `jsval_regexp_new_jit(...)` when the transpilation can see the regex
  will be reused long enough for JIT compile cost to pay back

Use the explicit JIT constructor only when the emitted C makes the longer
lifetime obvious and reviewable. Good JIT candidates include:

- one regex object constructed once and reused across repeated `exec` calls
- one global regex object constructed once and reused across multiple
  `matchAll` iterator constructions
- one hoisted direct-lowered regex reused across multiple replace-family
  operations

Stay on the default constructor for:

- one-shot constructor sites followed by a single regex operation
- compile-per-iteration or compile-per-call sites
- `jsval_regexp_new(existing_regexp, ...)` copy or flag-override sites unless
  the new regex is itself clearly hoisted and reused
- all rewrite-backed `/u` helper families, which do not lower through
  `jsval_regexp_new(...)`

Reviewable emitted-C examples:

- positive:
  `jsval_regexp_new_jit(..., &regex, ...)` once, then repeated
  `jsval_regexp_exec(..., regex, ...)`
- negative:
  `jsval_regexp_new(..., &regex, ...)` immediately followed by one
  `jsval_regexp_exec(..., regex, ...)`

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
  `matchAll`, `replace`, `replaceAll`, callback replacers, and `split`
- fixed multi-literal `/u` no-capture string-method behavior now has a
  reviewed rewrite-backed recipe covering `search`, `match`, `matchAll`,
  `replace`, `replaceAll`, callback replacers, and `split`
- single no-capture character classes of explicit literal UTF-16 members
  under `/u`, including negated literal-member classes, now have a reviewed
  rewrite-backed recipe covering `search`, `match`, `matchAll`, `replace`,
  `replaceAll`, callback replacers, and `split`
- single no-capture predefined `/u` classes (`\d`, `\D`, `\s`, `\S`, `\w`,
  `\W`) now have a reviewed rewrite-backed recipe covering `search`, `match`,
  `matchAll`, `replace`, `replaceAll`, callback replacers, and `split`
- broader Unicode/surrogate-sensitive `/u` behavior remains outside direct
  lowering until additional rewrite recipes are reviewed

## Current Known Gaps

| Family | Representative case | Current classification | Next action |
| --- | --- | --- | --- |
| Single literal lone-surrogate `/u` no-capture exec/test/search/match/matchAll/replace/replaceAll/split behavior | `strings/test262/exec/u-lastindex-adv` and `strings/jsmx/u-literal-surrogate-matchAll-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to literal no-capture string-method, iterator, and callback-replacer cases until more recipes are reviewed |
| Fixed multi-literal `/u` no-capture string-method behavior | `strings/jsmx/u-literal-sequence-match-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to exact UTF-16 literal sequences with no captures, classes, alternation, or quantifiers |
| Single no-capture character classes of explicit literal UTF-16 members under `/u`, including negated literal-member classes | `strings/jsmx/u-literal-negated-class-match-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to one class atom with explicit literal members only; negation is allowed, but ranges, escapes, alternation, and quantifiers remain out of scope |
| Single no-capture character classes of explicit inclusive UTF-16 ranges under `/u`, including negated ranged classes | `strings/jsmx/u-literal-negated-range-class-match-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to one class atom with explicit inclusive range data only; negation is allowed, but escapes, alternation, and quantifiers remain out of scope |
| Single no-capture predefined `/u` classes | `strings/jsmx/u-predefined-word-class-match-rewrite` | `Rewrite-backed:` | Keep the rewrite scoped to a single predefined class atom with JS-correct `\d` / `\w` ASCII semantics and JS WhiteSpace + LineTerminator `\s` semantics only |
| Broader Unicode/surrogate-sensitive `/u` behavior | future cases beyond explicit literal, class, ranged, or predefined no-capture `/u` cases | `Unsupported:` rewrite-candidate | Land additional reviewed rewrite recipes before translating them as passing cases |
| Reflective regex property-override behavior | `strings/test262/matchAll/flags-nonglobal-throws` | `Idiomatic slow path:` | Keep it as an explicit slow path unless the runtime grows a reflective regex object model |
| `d` / `v` flag surface | `regex/test262/flags/this-val-regexp` | `Unsupported:` beyond the current `gimsuy` subset | Add runtime/backend support before expanding coverage |

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

## Approved Rewrite: Fixed Multi-Literal No-Capture Sequence Under `/u`

Use a rewrite-backed lowering only when all of these are true:

- the JS pattern is a fixed UTF-16 literal sequence under the `u` flag
- the translated case only needs literal no-capture
  `search`/`match`/`matchAll`/`replace`/`replaceAll`/`split` semantics
- there are no captures, character classes, alternation, quantifiers,
  backreferences, or other regex operators in the pattern

Rewrite recipe:

- do **not** lower the pattern through `jsval_regexp_new(...)`
- emit the fixed literal pattern as UTF-16 data and lower string-method
  surfaces through the dedicated `jsval` helpers:
  - `jsval_method_string_search_u_literal_sequence(...)`
  - `jsval_method_string_match_u_literal_sequence(...)`
  - `jsval_method_string_match_all_u_literal_sequence(...)`
  - `jsval_method_string_replace_u_literal_sequence(...)`
  - `jsval_method_string_replace_all_u_literal_sequence(...)`
  - `jsval_method_string_replace_u_literal_sequence_fn(...)`
  - `jsval_method_string_replace_all_u_literal_sequence_fn(...)`
  - `jsval_method_string_split_u_literal_sequence(...)`
- match exact UTF-16 code-unit sequences only when the candidate start and end
  both fall on Unicode code-point boundaries
- scan the subject with Unicode code-point advancement semantics so intact
  surrogate pairs are skipped as indivisible code points during the search
- keep the same no-capture result contract as the current semantic regex layer:
  - `match` non-global returns an exec-shaped object or `null`
  - `match` global returns an array of matched strings or `null`
  - `matchAll` yields exec-shaped iterator results with `groups = undefined`
  - `replace`/`replaceAll` callback replacers receive `(match, offset, input)`

This rewrite is intentionally narrow. Any broader `/u` case, including
quantifiers, alternation, captures, or named groups, still needs a separate
reviewed recipe before it can be translated as passing.

## Approved Rewrite: Single No-Capture Character Class Under `/u`

Use a rewrite-backed lowering only when all of these are true:

- the JS pattern is exactly one character class atom under the `u` flag
- the class contains only explicit literal UTF-16 members supplied directly by
  the translator
- optional negation is allowed for the whole class atom, but the member set
  itself is still explicit literal data
- the translated case only needs no-capture
  `search`/`match`/`matchAll`/`replace`/`replaceAll`/`split` semantics
- there are no ranges, escapes with broader regex meaning, alternation,
  quantifiers, backreferences, or other regex operators
- empty member sets are outside the approved family; the translator should not
  lower them through this rewrite lane

Rewrite recipe:

- do **not** lower the pattern through `jsval_regexp_new(...)`
- emit the class members as UTF-16 data and lower string-method surfaces
  through the dedicated `jsval` helpers:
  - `jsval_method_string_search_u_literal_class(...)`
  - `jsval_method_string_match_u_literal_class(...)`
  - `jsval_method_string_match_all_u_literal_class(...)`
  - `jsval_method_string_replace_u_literal_class(...)`
  - `jsval_method_string_replace_all_u_literal_class(...)`
  - `jsval_method_string_replace_u_literal_class_fn(...)`
  - `jsval_method_string_replace_all_u_literal_class_fn(...)`
  - `jsval_method_string_split_u_literal_class(...)`
- for negated literal-member classes, use the parallel dedicated helpers:
  - `jsval_method_string_search_u_literal_negated_class(...)`
  - `jsval_method_string_match_u_literal_negated_class(...)`
  - `jsval_method_string_match_all_u_literal_negated_class(...)`
  - `jsval_method_string_replace_u_literal_negated_class(...)`
  - `jsval_method_string_replace_all_u_literal_negated_class(...)`
  - `jsval_method_string_replace_u_literal_negated_class_fn(...)`
  - `jsval_method_string_replace_all_u_literal_negated_class_fn(...)`
  - `jsval_method_string_split_u_literal_negated_class(...)`
- a class match consumes exactly one UTF-16 code unit at a Unicode code-point
  boundary
- intact surrogate pairs in the subject remain indivisible code points during
  scanning, so a class member must never match only the high or low half of a
  well-formed pair
- isolated surrogate units in the subject may match when their exact code unit
  appears in the class member set
- for negated literal-member classes, a match still consumes exactly one
  boundary-aligned UTF-16 code unit, but only when that isolated code unit is
  not in the explicit member set; intact surrogate pairs remain indivisible
  and are never consumed by the negated helper
- keep the same no-capture result contract as the current semantic regex layer:
  - `match` non-global returns an exec-shaped object or `null`
  - `match` global returns an array of matched one-unit strings or `null`
  - `matchAll` yields exec-shaped iterator results with `groups = undefined`
  - `replace`/`replaceAll` callback replacers receive `(match, offset, input)`

This rewrite is intentionally narrow. Any broader `/u` case, including
multi-atom classes with mixed operators, escapes, quantifiers, alternation,
captures, or named groups, still needs a separate reviewed recipe before it
can be translated as passing.

## Approved Rewrite: Single No-Capture Ranged Character Class Under `/u`

Use a rewrite-backed lowering only when all of these are true:

- the JS pattern is exactly one character class atom under the `u` flag
- the class is represented as one or more explicit inclusive UTF-16
  `[start, end]` range pairs supplied directly by the translator
- optional negation is allowed for the whole class atom
- the translated case only needs no-capture
  `search`/`match`/`matchAll`/`replace`/`replaceAll`/`split` semantics
- there are no escapes with broader regex meaning, alternation, quantifiers,
  backreferences, or other regex operators
- empty range tables are outside the approved family; the translator should
  not lower them through this rewrite lane

Rewrite recipe:

- do **not** lower the pattern through `jsval_regexp_new(...)`
- emit the class as explicit inclusive UTF-16 range data and lower
  string-method surfaces through the dedicated `jsval` helpers:
  - `jsval_method_string_search_u_literal_range_class(...)`
  - `jsval_method_string_match_u_literal_range_class(...)`
  - `jsval_method_string_match_all_u_literal_range_class(...)`
  - `jsval_method_string_replace_u_literal_range_class(...)`
  - `jsval_method_string_replace_all_u_literal_range_class(...)`
  - `jsval_method_string_replace_u_literal_range_class_fn(...)`
  - `jsval_method_string_replace_all_u_literal_range_class_fn(...)`
  - `jsval_method_string_split_u_literal_range_class(...)`
- for negated ranged classes, use the parallel dedicated helpers:
  - `jsval_method_string_search_u_literal_negated_range_class(...)`
  - `jsval_method_string_match_u_literal_negated_range_class(...)`
  - `jsval_method_string_match_all_u_literal_negated_range_class(...)`
  - `jsval_method_string_replace_u_literal_negated_range_class(...)`
  - `jsval_method_string_replace_all_u_literal_negated_range_class(...)`
  - `jsval_method_string_replace_u_literal_negated_range_class_fn(...)`
  - `jsval_method_string_replace_all_u_literal_negated_range_class_fn(...)`
  - `jsval_method_string_split_u_literal_negated_range_class(...)`
- a ranged-class match consumes exactly one UTF-16 code unit at a Unicode
  code-point boundary
- intact surrogate pairs in the subject remain indivisible code points during
  scanning, so a range must never match only the high or low half of a
  well-formed pair
- isolated surrogate units in the subject may match when their exact code unit
  falls inside one of the supplied inclusive ranges
- for negated ranged classes, a match still consumes exactly one
  boundary-aligned UTF-16 code unit, but only when that isolated code unit is
  not covered by the explicit range table; intact surrogate pairs remain
  indivisible and are never consumed by the negated helper
- keep the same no-capture result contract as the current semantic regex layer:
  - `match` non-global returns an exec-shaped object or `null`
  - `match` global returns an array of matched one-unit strings or `null`
  - `matchAll` yields exec-shaped iterator results with `groups = undefined`
  - `replace`/`replaceAll` callback replacers receive `(match, offset, input)`

This rewrite is intentionally narrow. Any broader `/u` case, including
escaped classes beyond the reviewed predefined lane, set operators,
alternation, quantifiers, captures, or named groups, still needs a separate
reviewed recipe before it can be translated as passing.

## Approved Rewrite: Single No-Capture Predefined Character Class Under `/u`

Use a rewrite-backed lowering only when all of these are true:

- the JS pattern is exactly one predefined class atom under the `u` flag
- the class is one of `\d`, `\D`, `\s`, `\S`, `\w`, or `\W`
- the translated case only needs no-capture
  `search`/`match`/`matchAll`/`replace`/`replaceAll`/`split` semantics
- there are no captures, alternation, quantifiers, backreferences, or other
  regex operators around the predefined atom

Rewrite recipe:

- do **not** lower the pattern through `jsval_regexp_new(...)`
- lower string-method surfaces through the dedicated `jsval` helpers:
  - `jsval_method_string_search_u_digit_class(...)`
  - `jsval_method_string_match_u_digit_class(...)`
  - `jsval_method_string_match_all_u_digit_class(...)`
  - `jsval_method_string_replace_u_digit_class(...)`
  - `jsval_method_string_replace_all_u_digit_class(...)`
  - `jsval_method_string_replace_u_digit_class_fn(...)`
  - `jsval_method_string_replace_all_u_digit_class_fn(...)`
  - `jsval_method_string_split_u_digit_class(...)`
  - `jsval_method_string_search_u_negated_digit_class(...)`
  - `jsval_method_string_match_u_negated_digit_class(...)`
  - `jsval_method_string_match_all_u_negated_digit_class(...)`
  - `jsval_method_string_replace_u_negated_digit_class(...)`
  - `jsval_method_string_replace_all_u_negated_digit_class(...)`
  - `jsval_method_string_replace_u_negated_digit_class_fn(...)`
  - `jsval_method_string_replace_all_u_negated_digit_class_fn(...)`
  - `jsval_method_string_split_u_negated_digit_class(...)`
  - `jsval_method_string_search_u_whitespace_class(...)`
  - `jsval_method_string_match_u_whitespace_class(...)`
  - `jsval_method_string_match_all_u_whitespace_class(...)`
  - `jsval_method_string_replace_u_whitespace_class(...)`
  - `jsval_method_string_replace_all_u_whitespace_class(...)`
  - `jsval_method_string_replace_u_whitespace_class_fn(...)`
  - `jsval_method_string_replace_all_u_whitespace_class_fn(...)`
  - `jsval_method_string_split_u_whitespace_class(...)`
  - `jsval_method_string_search_u_negated_whitespace_class(...)`
  - `jsval_method_string_match_u_negated_whitespace_class(...)`
  - `jsval_method_string_match_all_u_negated_whitespace_class(...)`
  - `jsval_method_string_replace_u_negated_whitespace_class(...)`
  - `jsval_method_string_replace_all_u_negated_whitespace_class(...)`
  - `jsval_method_string_replace_u_negated_whitespace_class_fn(...)`
  - `jsval_method_string_replace_all_u_negated_whitespace_class_fn(...)`
  - `jsval_method_string_split_u_negated_whitespace_class(...)`
  - `jsval_method_string_search_u_word_class(...)`
  - `jsval_method_string_match_u_word_class(...)`
  - `jsval_method_string_match_all_u_word_class(...)`
  - `jsval_method_string_replace_u_word_class(...)`
  - `jsval_method_string_replace_all_u_word_class(...)`
  - `jsval_method_string_replace_u_word_class_fn(...)`
  - `jsval_method_string_replace_all_u_word_class_fn(...)`
  - `jsval_method_string_split_u_word_class(...)`
  - `jsval_method_string_search_u_negated_word_class(...)`
  - `jsval_method_string_match_u_negated_word_class(...)`
  - `jsval_method_string_match_all_u_negated_word_class(...)`
  - `jsval_method_string_replace_u_negated_word_class(...)`
  - `jsval_method_string_replace_all_u_negated_word_class(...)`
  - `jsval_method_string_replace_u_negated_word_class_fn(...)`
  - `jsval_method_string_replace_all_u_negated_word_class_fn(...)`
  - `jsval_method_string_split_u_negated_word_class(...)`
- a predefined-class match consumes exactly one UTF-16 code unit at a Unicode
  code-point boundary
- intact surrogate pairs in the subject remain indivisible code points during
  scanning and are never consumed by the predefined helpers
- isolated surrogate units in the subject are non-members of the positive
  classes and may be matched only by the negated forms when boundary-aligned
- keep JS-correct set semantics:
  - `\d` is ASCII digits only
  - `\w` is ASCII word characters only: `A-Z`, `a-z`, `0-9`, `_`
  - `\s` is the JavaScript WhiteSpace + LineTerminator set
  - the negated forms are complements of those sets at valid boundaries
- keep the same no-capture result contract as the current semantic regex
  layer:
  - `match` non-global returns an exec-shaped object or `null`
  - `match` global returns an array of matched one-unit strings or `null`
  - `matchAll` yields exec-shaped iterator results with `groups = undefined`
  - `replace`/`replaceAll` callback replacers receive `(match, offset, input)`

This rewrite is intentionally narrow. Any broader `/u` case, including mixed
classes, escapes beyond this reviewed predefined lane, set operators,
alternation, quantifiers, captures, or named groups, still needs a separate
reviewed recipe before it can be translated as passing.

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

- `d` / match indices
- `v` / Unicode sets
- Unicode property escapes and broader escaped-character-class behavior beyond
  the reviewed predefined-class lane
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
