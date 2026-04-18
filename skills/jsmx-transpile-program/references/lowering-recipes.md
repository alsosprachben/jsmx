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

**TextEncoder / TextDecoder lowering**. jsmx exposes sync UTF-8
codec wrappers as utility functions, not object-model classes:

- `new TextEncoder()` erases at transpile time (no per-instance
  state in jsmx — UTF-8 is the only encoding).
- `encoder.encode(s)` → `jsval_text_encode_utf8(region, s, &out)`
  (output is a `Uint8Array` whose backing ArrayBuffer holds the
  UTF-8 bytes).
- `new TextDecoder()` also erases at transpile time (fatal /
  ignoreBOM / streaming are out of this slice's scope).
- `decoder.decode(buf)` → `jsval_text_decode_utf8(region, buf, &out)`
  where `buf` is a `Uint8Array` / `Int8Array` / `Uint8ClampedArray`
  / `ArrayBuffer`.

**`atob` / `btoa` lowering**. Standard-alphabet base64 with `=`
padding. jsmx's shape uses `Uint8Array` / `ArrayBuffer` on the
binary side (not the spec's Latin-1 `DOMString`), matching how
Node's `Buffer.from(str, 'base64')` is typically used in
WinterTC handlers:

- `btoa(bytes)` → `jsval_base64_encode(region, bytes, &out)`
  where `bytes` is a `Uint8Array` / `Int8Array` /
  `Uint8ClampedArray` / `ArrayBuffer`; output is a base64 jsval
  string (with `+`, `/`, `=`).
- `atob(s)` → `jsval_base64_decode(region, s, &out)` where `s`
  is a base64 jsval string; output is a `Uint8Array`.
  Base64url characters (`-`, `_`) are rejected by the standard
  decoder — use the internal base64url path for JWK transport.

**`queueMicrotask` lowering**. `queueMicrotask(fn)` →
`jsval_queue_microtask(region, fn)`, a thin named wrapper over
`jsval_microtask_enqueue` with `argc=0`. The callback runs on
the next `jsval_microtask_drain` pass; rejects non-function
arguments with -1.

## Objects, Arrays, Sets, And Maps

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
- sets:
  - `jsval_set_new(...)`
  - `jsval_set_size(...)`
  - `jsval_set_has(...)`
  - `jsval_set_add(...)`
  - `jsval_set_delete(...)`
  - `jsval_set_clear(...)`
  - `jsval_set_clone(...)` when the translator intentionally grows a
    capacity-bounded set
- maps:
  - `jsval_map_new(...)`
  - `jsval_map_size(...)`
  - `jsval_map_has(...)`
  - `jsval_map_get(...)`
  - `jsval_map_set(...)`
  - `jsval_map_delete(...)`
  - `jsval_map_clear(...)`
  - `jsval_map_key_at(...)`
  - `jsval_map_value_at(...)`
  - `jsval_map_clone(...)` when the translator intentionally grows a
    capacity-bounded map
- iterators:
  - `jsval_get_iterator(...)`
  - `jsval_iterator_next(...)`
  - `jsval_iterator_next_entry(...)`
  - default iterator mapping is:
    - strings -> values
    - arrays -> values
    - sets -> values
    - maps -> entries
  - use `next_entry(...)` for entry iterators instead of allocating pair
    arrays in the hot path
  - string values iterate with JS string iterator semantics:
    - surrogate pairs stay together
    - lone surrogates yield one single-unit string
    - keys/entries use UTF-16 code-unit offsets

Remember the current boundary:

- arrays are dense and capacity-bounded
- sets are insertion-ordered and capacity-bounded
- maps are insertion-ordered and capacity-bounded
- string iteration is available only through the explicit iterator helpers
- generic `Symbol.iterator` and iterable duck typing are still out of scope
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

## FaaS Fetch Handlers

When the entrypoint is a module that default-exports a
`{ async fetch(request) { ... } }` object (or equivalent
`addEventListener("fetch", ...)` form), lower it to a
**hosted-function handler** instead of a standalone `main(...)`. The
output file exports a single C function matching `faas_fetch_handler_fn`
from `runtime_modules/shared/faas_bridge.h`; the embedder supplies
`main(...)` and calls the handler via
`runtime_modules/shared/faas_bridge.h` helpers.

### Output Shape

- One JS entrypoint maps to one C source file with no `main(...)`.
- Exported symbol name: `<basename>_fetch_handler` matching
  `faas_fetch_handler_fn`:
  ```c
  int <basename>_fetch_handler(jsval_region_t *region,
          jsval_t request_value, jsval_t *response_promise_out);
  ```
- The handler signature is prototyped in `faas_bridge.h` (line 27–28) —
  do not redeclare it in the generated file.
- `#include` only `<errno.h>`, `<stddef.h>`, `<stdint.h>`, `<string.h>`,
  `"jsmethod.h"`, and `"jsval.h"`. The handler does not touch the FaaS
  bridge header itself — it just matches the signature.
- Header comment: embed the full JS source inline (or a faithful
  summary), state lowering class `production_slow_path`, and list host
  assumptions (transport registered, body prewarmed, upstream base
  baked in).

### CPS Lowering For `await`

Each `await fetch(...)` or `await response.<bodyMethod>()` site lowers
to a **continuation** — a separate static C function with the
`jsval_native_function_fn` signature. The outer handler:

1. Calls `jsval_fetch(...)` to get a pending upstream Promise.
2. Wraps the next continuation via `jsval_function_new(...)`.
3. Calls `jsval_promise_then(region, upstream_promise, continuation_fn,
   jsval_undefined(), &downstream_promise)` and returns the
   `downstream_promise` as the handler output.

The continuation is called by the jsmx microtask drain when the
upstream Promise fulfills. `argv[0]` holds the fulfilled value (the
upstream `Response`, the drained body ArrayBuffer, etc.). The
continuation returns the next-stage value via `*result_ptr`, which the
drain uses to fulfill the downstream Promise.

- **No explicit `on_rejected`** — rejections propagate through
  `jsval_promise_then` when `on_rejected` is `jsval_undefined()`,
  matching Promises/A+ semantics. The embedder's
  `faas_run_and_extract` will see the outer downstream Promise
  rejected and return `-1` with `errno=EIO`.
- **Live-across-await locals** go into either (a) a state struct
  allocated in the region and threaded through the continuation's
  userdata, or (b) fresh extractions in the continuation when every
  needed local can be recovered from the fulfillment value. Prefer
  (b) when possible — no closure allocation, lower CPS cost.

### Concurrent Fetches Via `Promise.all`

When the JS source issues two or more `fetch()` calls before
awaiting any of them — typically via explicit
`Promise.all([fetch(a), fetch(b), ...])` — lower the join via
`jsval_promise_all` rather than chained `.then` callbacks. Each
`jsval_fetch(...)` call synchronously parks a `(Request, Promise)`
entry on the region's fetch waitlist before the handler returns;
the embedder drains the whole waitlist in one dispatch cycle,
issuing each upstream request in parallel.

- **API**: `jsval_promise_all(region, inputs, n, &all_promise)`.
  Resolves with an `n`-element jsval array of results (input
  order) when all inputs fulfill; rejects with the first
  rejection reason if any input rejects. `n == 0` synchronously
  fulfills with the empty array. Re-entrant: multiple
  `jsval_promise_all` calls per region are tracked via a
  coordinator list scanned inside `jsval_microtask_drain`.
- **Lowering**: replace the chained-`.then` pattern with a single
  `jsval_promise_all` over the pending input promises, then
  register one continuation on the aggregate promise:

  ```c
  jsval_t inputs[2];
  /* each inputs[i] = jsval_fetch(...) result */
  if (jsval_promise_all(region, inputs, 2, &all_promise) < 0) return -1;
  if (jsval_function_new(region, on_both_fulfilled, 1, 0,
          jsval_undefined(), &continuation_fn) < 0) return -1;
  if (jsval_promise_then(region, all_promise, continuation_fn,
          jsval_undefined(), &downstream_promise) < 0) return -1;
  *response_promise_out = downstream_promise;
  ```

- **Continuation shape**: `argv[0]` is the results array; extract
  each entry via `jsval_array_get(region, results, i, &elem)`.
  Each entry is the upstream `Response`. From there, the **Body
  Consumption Shortcut** recipe applies per-entry — read the
  body via `jsval_response_array_buffer` + sync state check +
  result extract.
- **Body-level `Promise.all`**: if the JS source also wraps
  `[a.arrayBuffer(), b.arrayBuffer()]` in a `Promise.all`,
  collapse it inline rather than emitting a second
  `jsval_promise_all` — in-memory Response bodies are eagerly
  fulfilled, so the sync-extract idiom suffices per body. Same
  reasoning as the outer single-fetch body shortcut.
- **Rejection**: if any upstream fetch rejects, the aggregate
  promise rejects with that reason before the continuation runs.
  `jsval_promise_then` propagates the rejection into the
  downstream promise just as in the single-fetch path.
- **Sibling combinators**: `Promise.race` lowers to
  `jsval_promise_race(region, inputs, n, &out)` — output
  settles with the first input to transition from PENDING (in
  input order if multiple are already settled at call time).
  `Promise.allSettled` lowers to
  `jsval_promise_all_settled(region, inputs, n, &out)` — output
  fulfills with an array of `{status, value}` / `{status, reason}`
  descriptors once every input has settled, and never rejects.
  `Promise.any` lowers to
  `jsval_promise_any(region, inputs, n, &out)` — output resolves
  with the first fulfilled input's value; if all inputs reject,
  output rejects with `AggregateError(errors, "All promises were
  rejected")` where `errors` is the input-ordered array of
  rejection reasons. The AggregateError is produced via
  `jsval_aggregate_error_new`; consumers read
  `.name` / `.message` through the usual DOMException
  accessors and `.errors` through `jsval_dom_exception_errors`.
  All four combinators share the same continuation-registration
  shape — install a single `jsval_promise_then` handler on the
  combinator's output promise.

### Body Consumption Shortcut

`await response.arrayBuffer()` (and siblings `text() / bytes() /
json()`) is only pending when the Response has a **streaming body
source**. For in-memory Response bodies — which is what every current
transport produces, including `mock_transport_vtable` in `test_faas`
and the mnvkd `vk_jsmx_fetch_transport` plugin — the returned Promise
is already `JSVAL_PROMISE_STATE_FULFILLED` at the call site. The
lowering collapses:

```c
if (jsval_response_array_buffer(region, upstream, &body_promise) < 0) return -1;
if (jsval_promise_state(region, body_promise, &state) < 0) return -1;
if (state != JSVAL_PROMISE_STATE_FULFILLED) { errno = EIO; return -1; }
if (jsval_promise_result(region, body_promise, &body_value) < 0) return -1;
```

This is the same synchronous-extract idiom the mnvkd demo's
`hello_world_handler` uses for `jsval_request_text`
(`../mnvkd/vk_test_faas_demo.c:162–174`). It sidesteps a second CPS
continuation on the happy path. If streaming bodies land later, this
slot grows an additional continuation registered on `body_promise`
(same `jsval_promise_then` pattern as the outer fetch).

### Response Construction

`return new Response(body, { status, statusText, headers })` lowers
directly:

```c
if (jsval_object_new(region, 3, &init_value) < 0) return -1;
if (jsval_object_set_utf8(region, init_value,
        (const uint8_t *)"status", 6,
        jsval_number((double)status)) < 0) return -1;
/* ...statusText, headers... */
if (jsval_response_new(region, body_value, 1, init_value, 1,
        &response_value) < 0) return -1;
*result_ptr = response_value;
return 0;
```

`jsval_response_new`'s `have_body` and `have_init` booleans are both
`1` in the common case. The continuation returns the Response through
`*result_ptr`; the microtask drain uses that return value to fulfill
the outer downstream Promise.

### Outbound URL Construction

`request.url` from the FaaS bridge is the path-and-query component
only (matching mnvkd's `struct request::uri` shape). The
transpile-time constant upstream base is appended via
`jsval_method_string_concat(region, base_value, 1, &request_url,
&upstream_url, &error)`. Do **not** wrap it in `new URL(...)` —
absolute URL parsing is unnecessary for the proxy pattern and would
require the embedder to reconstruct an absolute inbound URL.

### Reference Output

Two canonical hand-lowered pairs live in `example/`:

- Single fetch: `example/wintertc_proxy.js` ↔
  `example/wintertc_proxy_handler.c`. End-to-end coverage in
  `test_faas.c` (`test_wintertc_proxy_handler_success` and
  `test_wintertc_proxy_handler_upstream_error`) against an
  in-process mock transport.
- Concurrent fetch via `Promise.all`:
  `example/wintertc_proxy_concurrent.js` ↔
  `example/wintertc_proxy_concurrent_handler.c`. End-to-end
  coverage in mnvkd's `vk_test_faas_demo_concurrent.passed`
  target, which spins up a real local upstream and verifies the
  concatenated response body.

Diff against the matching pair when extending the recipe.

### Build And Link

The handler file is a library-style C object, not a main. Link it into
the embedder binary alongside the jsmx core files:

```sh
cc -I. -c example/wintertc_proxy_handler.c -o wintertc_proxy_handler.o
# …then link wintertc_proxy_handler.o into whichever embedder main
# registers the fetch transport and calls faas_run_and_extract.
```

For the jsmx-side smoke, the `Makefile`'s `test_faas` target includes
`example/wintertc_proxy_handler.c` in the compile line. For the mnvkd
demo, `vk_test_faas_demo` adds the same file to its link. The
concurrent-fetch variant has its own mnvkd target
`vk_test_faas_demo_concurrent`, built with
`-DWINTERTC_PROXY_USE_CONCURRENT=1` so the demo's responder
forward-declares the concurrent handler in place of the single-fetch
one, and linking in `example/wintertc_proxy_concurrent_handler.c`
instead.

## Output And Error Handling

- keep exit behavior explicit
- propagate host failures through readable stderr messages
- prefer one clear failure path per helper call
- when the source JS throws semantics that the current runtime cannot model honestly, stop and mark the program `manual_runtime_needed` instead of faking exception behavior
