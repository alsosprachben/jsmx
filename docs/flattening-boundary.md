# Flattening Boundary

## Goal

`jsmx` is the flattened, page-resident semantic layer for a JS-to-C translator. It is not meant to become a general dynamic JavaScript runtime.

The design target is:

- keep hot data in contiguous caller-owned pages
- keep APIs explicit about sizing, promotion, and failure
- lower as much JavaScript behavior as possible into deterministic C
- push late-bound or host-dependent behavior out of `jsmx`

## Ownership

### `jsmx`

`jsmx` owns the semantics that can be expressed as explicit data transforms over caller-provided memory:

- strings, Unicode, casing, normalization, and well-formedness
- page-resident values, objects, arrays, and JSON-backed reads
- explicit promotion from JSON-backed storage to native storage
- dense, capacity-bounded native array semantics such as push, pop, shift, and explicit length writes
- deterministic JS-method helpers such as `String.prototype.normalize`

### Translator

The translator decides whether an operation can be flattened.

It is responsible for:

- choosing native vs JSON-backed storage
- measuring and providing exact scratch/result storage
- emitting explicit promotion on first semantic mutation
- choosing final native object/array capacities up front rather than relying on clone-to-bigger repair paths
- selecting specialized helpers when a direct lowering exists
- routing non-flattenable behavior to a separate slow path

### Slow Path

Dynamic behavior that depends on user-defined or late-bound semantics should stay outside `jsmx`.

Examples:

- prototype-sensitive method dispatch
- accessors and descriptor-heavy semantics
- `Symbol.toPrimitive` or custom coercion hooks
- proxies, `eval`, `with`, and similar dynamic escape hatches

## Capability Classes

Compliance entries and translator planning should use these classes:

- `static_pass`: the behavior can be lowered entirely through current `jsmx` and `jsmethod` surfaces
- `slow_path_needed`: the behavior is valid JS, but flattening would require dynamic runtime behavior outside the current `jsmx` model
- `unsupported`: the behavior is intentionally outside the current project scope

`expected_status` in the compliance manifest remains an execution result. `lowering_class` describes why the test fits, or does not fit, the flattened layer. `translation_mode` says whether the committed fixture demonstrates an idiomatic flattened lowering or an idiomatic slow path.

## Rule Of Thumb

If the translator can compute the needed storage, select the target helper, and preserve semantics without hidden callbacks, the behavior belongs in `jsmx`.

If correct behavior depends on dynamic object semantics, hidden hooks, or general runtime dispatch, the translator should emit a slow path instead of teaching `jsval` to become a dynamic object system.

Dense native arrays fit that flattened model. So do shallow promotion patterns where the translator promotes only the object or array that will mutate and leaves surrounding child values JSON-backed until they are explicitly selected. Hole-preserving `delete arr[i]`, sparse-index behavior, and other semantics that depend on distinguishing absent elements from explicit `undefined` values remain outside the current slice.

When the corpus includes those cases, prefer an idiomatic slow-path translation that still passes over a boundary-only `KNOWN_UNSUPPORTED` placeholder, unless the required slow-path contract has not been designed yet.
