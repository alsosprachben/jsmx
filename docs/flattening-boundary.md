# Flattening Boundary

See also [`fetch.md`](fetch.md) for how this boundary applies to the
WHATWG Fetch API: `jsmx` owns the JS object model (`Headers`, `Request`,
`Response`, `Body`), `mnvkd` owns the HTTP/1.1 parser and transport,
and `ReadableStream` is intentionally not a runtime value.

## Goal

`jsmx` is the flattened, page-resident semantic layer for a JS-to-C
translator.

It is not meant to become a general dynamic JavaScript runtime.

The design target is:

- keep hot data in contiguous caller-owned pages
- keep APIs explicit about sizing, promotion, and failure
- lower as much JavaScript behavior as possible into deterministic C
- push late-bound or host-dependent behavior out of `jsmx`

## Ownership

### `jsmx`

`jsmx` owns semantics that can be expressed as explicit data transforms over
caller-provided memory:

- strings, Unicode, casing, normalization, and well-formedness
- page-resident values, objects, arrays, Symbols, BigInts, Dates, and JSON-backed
  reads
- explicit promotion from JSON-backed storage to native storage
- deterministic ordered object key / value access
  - over current stored or parsed order
  - including symbol-keyed native own properties in insertion order
- shallow own-property copy
  - from native or JSON-backed source objects
  - into pre-capacity-planned native destinations
  - preserving symbol-keyed native own properties when present
- fresh own-property clone
  - from native or JSON-backed source objects
  - into a newly allocated pre-capacity-planned native destination
- translator-visible object / array spread-style lowerings
  - composed from shallow clone, copy, and dense append over explicit
    capacities
- dense, capacity-bounded native array semantics such as:
  - fresh dense-array clone
  - bounded dense-array splice edits
  - push
  - pop
  - shift
  - unshift
  - explicit length writes
  - explicit iterator production and stepping over values, keys, and entries
- native, insertion-ordered, capacity-bounded `Set` semantics such as:
  - `size`
  - `has`
  - `add`
  - `delete`
  - `clear`
  - clone-to-bigger repair when the translator intentionally grows a set
  - explicit iterator production and stepping over values, keys, and entries
- native, insertion-ordered, capacity-bounded `Map` semantics such as:
  - `size`
  - `has`
  - `get`
  - `set`
  - `delete`
  - `clear`
  - ordered key/value access
  - clone-to-bigger repair when the translator intentionally grows a map
  - explicit iterator production and stepping over keys, values, and entries
- deterministic JS-method helpers such as `String.prototype.normalize`
- native `BigInt` numeric core such as:
  - explicit construction from signed/unsigned integers and decimal UTF-8
  - exact decimal stringify / compare helpers
  - value-based strict and abstract equality
  - relational ordering against bigint, string, and numeric operands
  - page-resident `+`, `-`, `*`, and unary `-`
  - explicit mixed Number/BigInt rejection rather than hidden coercion
- native static function values such as:
  - translator-emitted capture-free static call targets
  - explicit `name`, `length`, and call helpers
  - first-class storage in native objects, arrays, `Set`, and `Map`
  - identity-sensitive equality without broadening into generic object
    dispatch or constructor semantics
- native `Date` object semantics such as:
  - epoch-millisecond storage with explicit invalid-date `NaN` state
  - bounded ISO parse plus explicit `Date.now()` / `Date.UTC(...)` helpers
  - UTC and host-local field getters/setters
  - ISO / UTC / local / JSON stringification helpers
  - host-libc local-time rules rather than repo-owned timezone data
- explicit native string iterator production and stepping such as:
  - values grouped by JS string iterator code-point semantics
  - keys and entries reported in UTF-16 code-unit offsets
  - no snapshot array materialization during stepping

### Translator

The translator decides whether an operation can be flattened.

It is responsible for:

- choosing native vs JSON-backed storage
- measuring and providing exact scratch/result storage
- emitting explicit promotion on first semantic mutation
- choosing final native object/array/set/map capacities up front rather than relying on clone-to-bigger repair paths
- selecting specialized helpers when a direct lowering exists
- routing non-flattenable behavior to a separate slow path

### Slow Path

Dynamic behavior that depends on user-defined or late-bound semantics should
stay outside `jsmx`.

Examples:

- prototype-sensitive method dispatch
- accessors and descriptor-heavy semantics
- `Symbol.toPrimitive` or custom coercion hooks
- JS-visible `BigInt()` constructor semantics beyond the explicit helper layer
- broad legacy Date-string parsing and host-independent timezone policy beyond
  the explicit ISO and libc-local helpers
- bigint division, remainder, bitwise, and shift semantics
- closures, dynamic `this`, `arguments`, constructor calls, and bound-function
  protocol semantics
- proxies, `eval`, `with`, and similar dynamic escape hatches

## Capability Classes

Compliance entries and translator planning should use these classes:

- `static_pass`
  - the behavior can be lowered entirely through current `jsmx` and `jsmethod`
    surfaces
- `slow_path_needed`
  - the behavior is valid JS
  - flattening would require dynamic runtime behavior outside the current
    `jsmx` model
- `unsupported`
  - the behavior is intentionally outside the current project scope

Manifest fields:

- `expected_status`
  - remains an execution result
- `lowering_class`
  - describes why the test fits, or does not fit, the flattened layer
- `translation_mode`
  - says whether the committed fixture demonstrates:
    - an idiomatic flattened lowering
    - an idiomatic slow path

## Rule Of Thumb

If the translator can compute the needed storage, select the target helper, and
preserve semantics without hidden callbacks, the behavior belongs in `jsmx`.

If correct behavior depends on:

- dynamic object semantics
- hidden hooks
- general runtime dispatch

the translator should emit a slow path instead of teaching `jsval` to become a
dynamic object system.

Dense native arrays fit that flattened model.

So do shallow promotion patterns where the translator:

- promotes only the object or array that will mutate
- leaves surrounding child values JSON-backed until they are explicitly
  selected

These remain outside the current slice:

- hole-preserving `delete arr[i]`
- sparse-index behavior
- other semantics that depend on distinguishing absent elements from explicit
  `undefined` values

When the corpus includes those cases:

- prefer an idiomatic slow-path translation that still passes
- avoid a boundary-only `KNOWN_UNSUPPORTED` placeholder
- unless the required slow-path contract has not been designed yet
