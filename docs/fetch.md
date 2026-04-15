# Fetch API Design

## Goal

Let hosted JavaScript express `fetch`, `Request`, `Response`, `Headers`, and
the `Body` mixin — enough to run OAuth flows, verify JWTs against an IdP's
JWKS endpoint, and implement billable WinterTC-style hosted functions on top
of mnvkd — without dragging a general-purpose asynchronous runtime into
`jsmx`.

The design splits along an ownership line plus two directional seams:

- **`jsmx`** owns the WHATWG Fetch *JS object model* and all HTTP-level
  semantics that can be expressed as deterministic data transforms over
  caller-owned memory. It also owns the **FaaS handler contract** — the
  idiom that a hosted function *is itself* a `(Request) => Promise<Response>`
  function, matching Cloudflare Workers, Deno Deploy, Bun.serve, Fastly
  Compute, and the service-worker family.
- **`mnvkd`** owns the HTTP/1.1 wire protocol, the coroutine scheduler, and
  every byte that touches a socket.

The two handoff points are **directional** and not symmetric today:

- **Inbound** (hosted function receives a request): `mnvkd`'s
  `vk_http11` is a mature coroutine-based server parser. It already
  yields on partial input, streams body bytes through `vk_forward`,
  and has a response writer. The integration seam is a header-emit
  callback into `jsval` plus a body-source adapter — described below.
- **Outbound** (hosted function calls `fetch()` to reach an external
  API, e.g. a JWKS endpoint): `mnvkd`'s `vk_fetch.c` is a *partial*
  HTTP client — it sketches a request serializer but its response
  handler is effectively a stub. The outbound story is therefore
  still an open design question (finish `vk_fetch.c` on the `mnvkd`
  side vs. have `jsmx` own the outbound state machine and drive
  `mnvkd` through pure `connect` / `read` / `write` / `close`). See
  [The outbound `fetch()` path](#the-outbound-fetch-path).

The key symmetry that makes this tractable: **the same Fetch API is
both the inbound handler contract and the outbound primitive.** One
vocabulary, used in both directions. That is why the Fetch JS object
model is the foundation for everything else here.

## Non-goals

- A generic async runtime in `jsmx`. Event loops, timers, and host I/O stay
  with `mnvkd` or the embedder.
- A generic WHATWG `ReadableStream` / `WritableStream` runtime value. See
  [Streaming bodies](#streaming-bodies) for the alternative.
- Mirroring `mnvkd`'s `struct request` / `struct vk_fetch` into JS. The
  `mnvkd` structs are a sizing compromise for its fixed-page allocator; they
  are not the right shape for a JS value.
- CORS preflight, redirect chain handling, cookie jars, or `Set-Cookie`
  parsing as runtime behavior. Those are either transpile-time concerns or
  userland library concerns.

## Ownership split

### `jsmx` owns

- `Headers`, `Request`, `Response`, and the `Body` mixin as first-class
  `jsval` kinds (`JSVAL_KIND_HEADERS`, `JSVAL_KIND_REQUEST`,
  `JSVAL_KIND_RESPONSE`).
- HTTP method / header / version / guard / request-mode enum vocabulary,
  mirrored byte-for-byte from `mnvkd/vk_fetch_s.h` so the two sides can
  exchange enum values as ABI without translation.
- Header name/value validation per RFC 9110 §5.6.2 (token grammar) and
  WHATWG Fetch §4.5 (whitespace normalization, forbidden-method rejection,
  `Set-Cookie` special-casing in `get()` and `getSetCookie()`).
- Body storage as an in-memory `ArrayBuffer` jsval, and the synchronous
  body-consumption promises (`text`, `json`, `arrayBuffer`, `bytes`).
- The `fetch()` entry point, which parses its arguments into a `Request`
  value and hands the request to the transport.

### `mnvkd` owns

**For inbound requests (server path, mature):**

- `vk_http11_request()` — the iterative request parser. Already
  coroutine-aware, yields on partial input, streams body bytes through
  `vk_forward` without copying them into a struct.
- `vk_http11_response()` — the response writer. Handles status line,
  headers, and chunked body framing.

**For outbound requests (client path, partial):**

- `vk_fetch.c` sketches an HTTP/1.1 client: it builds a request line
  and headers from a `struct vk_fetch` and streams the body through
  chunked encoding. The response handler is a stub (empty loop). A
  real outbound path requires either finishing this file or having
  `jsmx` own the client-side state machine — see the outbound section.

**Direction-agnostic:**

- The `vk_socket` / `vk_vectoring` / `vk_pipe` transport layer: rings,
  splices, half-close, backpressure.
- TLS termination, connect, accept, and all socket lifetime.
- The coroutine scheduler and the process-level I/O queue.

### What moves

- `mnvkd`'s fixed-size `struct request` header array (15 headers × 32-byte
  keys × 96-byte values) is too small for real-world traffic. Real
  `Authorization` bearer tokens and `Cookie` headers routinely exceed 96
  bytes. **This slice moves header storage out of the `mnvkd` struct and
  into a `jsval` `Headers` object.** The `vk_fetch_s.h` enums stay shared
  ABI; the storage shape does not.

## Inbound integration seam: the header-emit hook

Today `vk_http11_request()` copies each parsed header into
`struct request::headers[i]`. After this design, the parser accepts an
**emit callback** that is called once per parsed header, inside the
coroutine context:

```c
typedef int (*vk_http11_header_emit_fn)(
    void *userdata,
    const char *name, size_t name_len,
    const char *value, size_t value_len);
```

The `jsmx` side provides a callback that translates `(name, value)` into a
`jsval_headers_append(...)` call. Because `jsval_headers_append` is an
O(1) region-bump allocator plus two memcpys, the callback is fast and
never yields — safe to run inside the parser's coroutine.

**Two problems get fixed at once:**

1. The integration point for `jsmx` becomes a single function pointer
   instead of a translation pass.
2. `mnvkd`'s real-world header-size limits go away, because the `jsval`
   `Headers` grows with the region.

The request lifecycle then looks like:

```
accept()
  └─ allocate jsval_region_t for this request
       └─ vk_http11_request(parser, &region, emit_into_headers)
            └─ parser yields on partial reads (unchanged)
            └─ each header → jsval_headers_append
       └─ construct jsval Request { url, method, headers, body=pending }
       └─ hand Request to userland
            └─ userland: const body = await req.text()
                 └─ resolves through FETCH_BODY_DRAIN microtask
            └─ userland: return new Response(...)
       └─ serialize Response to socket
       └─ free region
```

The region is the unit of billing and memory metering, which matches how
a hosted-function runtime wants to account for requests.

## Body handoff: `FETCH_BODY_DRAIN` microtask

`mnvkd` already streams body bytes through `vk_forward` without copying
them into the parser struct. The Fetch spec, in practice, almost always
**slurps** the body (`await req.text()`, `await req.json()`, `await
req.arrayBuffer()`). The integration should exploit that.

A new microtask kind joins the existing crypto family:

```c
JSVAL_MICROTASK_KIND_FETCH_BODY_DRAIN
```

The microtask owns:

- a pointer (or handle) to the `mnvkd` body coroutine / vk_socket RX
- the destination `jsval` `ArrayBuffer` being grown in the region
- the target Promise
- a framing mode (`content-length`, `chunked`, `eof`)
- the remaining byte budget

When userland calls `req.text()` / `res.arrayBuffer()` / etc.:

1. A Promise is constructed immediately.
2. A `FETCH_BODY_DRAIN` microtask is enqueued.
3. The drain microtask pumps `vk_forward`-style reads into the growing
   `ArrayBuffer` until the framing terminator is hit (content-length
   reached, chunked final chunk, or half-close) or the byte budget is
   exceeded (in which case the Promise rejects with `TypeError`).
4. On completion, the Promise is resolved; on error, it is rejected.
5. Any subsequent `text()` / `arrayBuffer()` call rejects with
   `TypeError("body already used")`, matching spec semantics.

**Why a dedicated microtask kind and not a reused crypto kind?** The
crypto microtasks resolve synchronously against in-process state. The
body drain is the first microtask whose resolution actually depends on
external I/O. It needs to cooperate with the `mnvkd` scheduler, not
with `jsmx`'s synchronous compute loop, so its drain step is a
"pump and yield" — not a "compute and resolve."

### Streaming-into-slurp stays streaming under the hood

Even though the user-visible surface is slurp, the microtask pumps
bytes from `vk_forward` as they arrive. A 10 MB upload does not block
the parser; the drain microtask yields whenever the ring is empty.
Slurp is a userland concept. The runtime never actually holds the
whole body in a single kernel read.

### Backpressure

`vk_vectoring`'s `rx_blocked` / `tx_blocked` flags handle backpressure
naturally. When the drain microtask cannot pull because the ring is
full or empty, the coroutine blocks. The `jsmx` microtask queue treats
this as "not runnable yet" and drains the other microtasks (resolved
crypto, unrelated promise reactions) first. No explicit negotiation.

## FaaS handler contract

A hosted function *is* a `(Request) => Promise<Response>` function.
This is the idiom every modern JS FaaS platform has converged on:

```js
/* Cloudflare Workers / Deno Deploy / Bun.serve / Vercel Edge / etc. */
export default {
  async fetch(request, env, ctx) {
    const body = await request.json();
    return Response.json({ echo: body });
  }
};
```

or the equivalent service-worker form:

```js
addEventListener("fetch", (event) => {
  event.respondWith(handleRequest(event.request));
});
```

Both shapes collapse to the same contract: take a `Request`, return a
`Response` (or a `Promise<Response>`). The runtime's job is to invoke
the handler, await the promise, and serialize the result back out.

### Transpiler lowering

The transpiler recognizes the `export default { fetch(request) { ... } }`
and `addEventListener("fetch", ...)` idioms and lowers the user's handler
body into a single C function with a fixed signature:

```c
/*
 * Emitted by the transpiler from the user's default export.
 * The embedder calls this after parsing the request; it is expected
 * to return a Promise<Response> (via *response_promise_out) that
 * eventually settles on a Response value.
 *
 * Return 0 on normal completion (including thrown/rejected paths —
 * those resolve through the promise). Return -1 only on fatal runtime
 * errors that abort the request entirely.
 */
int user_fetch_handler(jsval_region_t *region,
        jsval_t request, jsval_t *response_promise_out);
```

The handler can freely:

- `await request.text() / .json() / .arrayBuffer() / .bytes()` —
  resolves through the `FETCH_BODY_DRAIN` microtask against the
  inbound body source.
- call `fetch("https://...")` — resolves through the outbound path
  (once it exists).
- perform crypto, construct values, run business logic.
- `return new Response(...)` or `return Response.json(...)`.

The returned Promise is what the embedder waits on. The handler does
**not** directly touch the response socket; it produces a Response
*value* and the embedder serializes it.

### The per-request lifecycle

The `mnvkd` embedder owns the outer loop. One request looks like:

```
accept()
  └─ allocate jsval_region_t for this request (billing unit)
       └─ jsval_headers_new(region, REQUEST) → headers_value
       └─ vk_http11_request(parser, emit_into_headers, &ctx)
            └─ parser yields on partial reads
            └─ each header → jsval_fetch_emit_header_into
       └─ construct a vk_http11 body source (vtable over vk_socket RX)
       └─ jsval_request_new_from_parts(region, method, url, headers,
              &vk_body_vtable, body_userdata, content_length,
              &request_value)
       └─ call user_fetch_handler(region, request_value,
              &response_promise)
       └─ drive jsval_microtask_drain(region, &error) until the
              response_promise settles
              ├─ drain pumps FETCH_BODY_DRAIN (inbound body)
              ├─ drain pumps crypto microtasks (JWT verification, etc.)
              ├─ drain pumps outbound FETCH_REQUEST microtasks if the
              │  handler called fetch() — see the outbound section
              └─ drain yields the embedder coroutine on PENDING
       └─ read resolved Response value from the promise
            ├─ if rejected: synthesize a 500 Response with the error
            │  message (or let a `try/catch` inside the handler
            │  produce a user-visible response)
            └─ if fulfilled: the handler's Response value
       └─ vk_http11_response writer serializes:
            ├─ status line from Response.status / statusText
            ├─ headers emitted from the Response's jsval Headers
            └─ body drained from the Response body source or copied
               from an eager ArrayBuffer
       └─ free the entire region
```

The region is the unit of billing, memory metering, and cleanup. One
region per request; freed at the end regardless of outcome. This is
exactly the shape every FaaS platform wants.

### Who drives the microtask drain

The embedder is the drain driver. It calls `jsval_microtask_drain`
in a loop, alternating with scheduler yields when the drain reports
that all remaining tasks are blocked on I/O. The existing
`jsval_region_set_scheduler` hook is the integration point: the
`on_wake` callback fires when a body source or outbound fetch becomes
ready, telling the embedder to re-enter `jsval_microtask_drain`.

Concretely, the embedder loop looks like:

```c
jsval_region_set_scheduler(region, &scheduler);
if (user_fetch_handler(region, request, &response_promise) < 0) {
    /* fatal — send 500 */;
}
while (jsval_promise_state_is_pending(region, response_promise)) {
    if (jsval_microtask_drain(region, &error) < 0) {
        break;
    }
    if (still_pending) {
        vk_yield_until_scheduler_wakes();
    }
}
```

This is the same loop shape every async runtime uses. The only unusual
thing about it is that it runs inside a `mnvkd` coroutine, so
"yield until scheduler wakes" is a cheap coroutine switch rather than
an OS-level poll.

### Error handling

Three distinct failure modes, each with a well-defined shape:

1. **Handler throws / returns rejected Promise**: the embedder reads
   the rejection reason from the promise, converts it to a 500-class
   Response (with a text body containing the exception name and
   message), and serializes that. Userland code that wants more
   control should `try/catch` and produce its own Response.
2. **Handler returns something that isn't a Response**: the embedder
   synthesizes a 500 Response with "handler did not return a Response"
   as the message.
3. **Fatal runtime error** (handler returned -1, region exhausted,
   microtask drain errored): the embedder closes the connection
   without a response. Logged at the embedder level.

### Environment and context bindings

Cloudflare-style `fetch(request, env, ctx)` exposes two additional
parameters: `env` (environment variables, KV bindings, secret
material) and `ctx` (waitUntil / passThroughOnException hooks).
For the first slice, the transpiler lowers only the single-argument
form `fetch(request)`. Env and ctx are a later concern — they
naturally map to a pair of pre-constructed jsval objects the
embedder hands to the handler, but the interesting design question
is **what goes in env** (secrets? KV? config?) and that is
deployment-platform-specific, not a Fetch concern.

### Why this handler shape and not a plain `main()`

A plain `main()` that reads stdin and writes stdout would be simpler
but loses the key FaaS properties:

- **Multiple requests per process.** One handler invocation per
  request; the compiled binary stays resident and bills per-request.
- **Per-request region isolation.** Every request gets a fresh
  `jsval_region_t` so there is no cross-request state leakage.
- **Async composition.** A handler can `await` multiple things and
  the drain handles them in one shared microtask queue.
- **Symmetric outbound.** `fetch(url)` inside the handler uses the
  same object model as the inbound request, and its promise lives
  in the same microtask queue.

The handler-per-request shape is what makes the runtime *useful* to
the JS author; it's not an implementation detail.

## The outbound `fetch()` path

`fetch(input, init?)` today is a reject-stub that parses input/init
into a `Request` and returns a Promise rejected with `TypeError("network
not implemented yet")`. Making it real is the second-biggest remaining
piece of work after the FaaS handler contract.

### Open design question: who owns the client state machine?

Two viable options, not yet decided:

**Option A — finish `vk_fetch.c` on the `mnvkd` side.** `mnvkd` already
has a partial HTTP/1.1 client sketch. Completing it (a real response
parser mirroring `vk_http11_request()`, plus chunked body framing)
gives `jsmx` a single-function entry point to call. The `jsmx` side
stays thin: hand the client a method, URL, headers snapshot, and a
request body source; receive a response struct plus a body source.

- **Pros**: reuses `mnvkd`'s proven coroutine parser pattern for the
  response path. Keeps all wire-protocol code on the C side where the
  I/O primitives live.
- **Cons**: two parsers to maintain on the `mnvkd` side (one for
  inbound server requests, one for outbound client responses). Cross-repo
  coupling on every protocol feature.

**Option B — `jsmx` owns the client state machine, drives `mnvkd`
through pure I/O.** `jsmx` implements an HTTP/1.1 request serializer
and response parser, talking to `mnvkd` only through `connect` /
`read` / `write` / `close` on a `vk_socket`. This matches the earlier
architectural intuition that `jsmx` should own protocol-level semantics.

- **Pros**: single-repo protocol implementation, testable end-to-end
  against an in-memory byte pipe. Symmetric with how the inbound body
  drain already works (it reads bytes through a vtable). `mnvkd`'s
  role is purely transport.
- **Cons**: `jsmx` grows an HTTP/1.1 response parser — roughly 400
  lines of state machine plus tests. Feature parity with `vk_http11`
  has to be maintained manually.

Both options resolve to the same JS-visible behavior, so the decision
doesn't block the inbound FaaS demo. The outbound fetch stub can stay
in place through several slices of inbound work without blocking OAuth
or JWKS — that work has to wait on outbound anyway.

### Shared invariants regardless of option

Whichever path we pick, these properties hold:

- **The reply's body is drained through the same `FETCH_BODY_DRAIN`
  microtask kind as the inbound body.** Both directions share one
  `jsval_body_source_vtable_t` implementation surface; only the
  userdata differs (inbound = vk_socket RX plus content-length
  framing; outbound = vk_socket RX plus the response framing the
  client state machine parsed out of the reply).
- **The response headers land in a `jsval` Headers through the same
  header-emit callback shape** — `jsval_fetch_emit_header_into` is
  direction-agnostic.
- **`jsval_fetch()` returns a Promise that eventually resolves to a
  `Response` value.** The embedder's microtask drain loop is the
  same loop that handles body consumption and crypto; outbound
  fetch is one more microtask kind that yields until its socket is
  ready.
- **Redirect handling, cookie jars, and cache are explicitly not
  runtime features.** See below.

### Redirects

`redirect: "follow"` (the default) means the `FETCH_REQUEST` microtask
does not resolve the outer Promise on a 3xx response. It re-enters
its own state machine with the `Location` header as the new target,
up to a bounded hop count. `"error"` rejects on any 3xx. `"manual"`
resolves with the 3xx `Response` directly.

### Cookie jar, CORS, cache

Out of scope for this design. A hosted function that needs cookies
imports a userland cookie-jar library that stores state between
`fetch()` calls. CORS does not apply to a server runtime. HTTP cache
is a separate design question.

## Streaming bodies

WHATWG Fetch exposes `ReadableStream` and `WritableStream` on
`Request.body` and `Response.body`. For jsmx these are **not** runtime
values. The reasoning:

1. **`vk_socket` and `ReadableStream` have different ontologies.**
   `vk_socket` wants a graph built up front — coroutines, rings,
   `VK_OP_FORWARD` splices — then drained. `ReadableStream` wants a
   graph the JS program can rewire between chunks via
   `.pipeThrough()`. Those are not different levels of sophistication;
   they are different assumptions about *when* the graph is known.

2. **`jsmx` is a transpiler.** Its strongest tool is recognizing
   static patterns and lowering them to dedicated C ops. Dynamic
   stream composition is exactly the kind of late-bound behavior the
   flattening boundary is built to refuse.

3. **Fetch is 99% slurp.** Real hosted-function code rarely touches
   the streaming path. The cost of a generic `ReadableStream`
   runtime object (closure capture for `pull()`, reentrancy rules,
   cancellation negotiation, Symbol.asyncIterator protocol) buys
   almost nothing for that one percent.

The alternative is a small set of **dedicated verbs** that lower
directly to `vk_socket` ops at transpile time. Candidates for the
first streaming slice:

- `response.body.forEachChunk(chunk => ...)` → `vk_socket` read loop,
  no allocation churn
- `response.body.pipeToFile(path)` → `vk_vectoring_splice` to a file
  fd, zero-copy
- `response.body.toArrayBuffer()` → slurp (the object-model path
  this slice shipped)
- `new Response(asyncIterator)` where the iterator is a recognizable
  generator pattern → producer coroutine owning a `vk_socket` TX

These verbs are not a replacement for WHATWG streams. They are the
set of *operations over streams* that the transpiler can prove static
and lower without a runtime graph. Userland code that tries to build
a dynamic pipeline graph falls into `manual_runtime_needed`, the
same way closures and `Symbol.iterator` protocol already do.

**What is explicitly ruled out:**

- `new ReadableStream({pull, cancel})` with user-defined `pull()` —
  would require closure capture + reentrancy rules inside the
  coroutine model.
- `rs.pipeThrough(transform).pipeTo(ws)` as generic runtime composition
  — would require a dynamic graph representation.
- Exposing `vk_socket` as a `jsval` kind — would drag `mnvkd`'s
  coroutine blocking model into the JS value world, where it can't be
  expressed naturally.

## Ordering of future slices

**Landed:**

1. ✅ **Fetch JS object model** — `Headers`, `Request`, `Response`,
   `Body` mixin, `fetch()` reject-stub. 7 compliance fixtures.
2. ✅ **Fetch transport foundation** — `jsval_body_source_vtable_t`,
   `JSVAL_MICROTASK_KIND_FETCH_BODY_DRAIN`, lazy Request/Response
   bodies, `jsval_request_new_from_parts`, `jsval_response_new_from_parts`,
   `jsval_fetch_emit_header_into` reference callback. All testable
   in jsmx with a fake body source; no mnvkd patches.
3. ✅ **FaaS handler bridge** —
   `runtime_modules/shared/faas_bridge.h` defines the
   `faas_fetch_handler_fn` contract, `faas_drain_until_settled`,
   and `faas_serialize_response`. Plus `jsval_request_prewarm_body`
   / `jsval_*_body_snapshot` helpers. Nine scenarios tested in
   `test_faas`.

**Pending:**

4. **mnvkd header-emit hook** — cross-repo. Patch
   `vk_http11_request()` to accept a `vk_http11_header_emit_fn` and
   invoke it once per parsed header. Keep the existing
   `struct request` path as a default callback so mnvkd's current
   tests don't change. This is a small, surgical patch — the shape
   is already locked by the jsmx-side types.
5. **mnvkd body-source adapter** — a `jsval_body_source_vtable_t`
   implementation that pulls bytes from a `vk_socket` RX ring honoring
   `Content-Length` or chunked framing. Lives alongside the
   hosted-function runner on the mnvkd side.
6. **`vk_test_faas_demo.c`** — cross-repo hello-world demo binary.
   Copies `vk_test_http11_service.c`, replaces the hardcoded
   response with a `faas_fetch_handler_fn` call through
   `runtime_modules/shared/faas_bridge.h`, and writes the
   serialized Response into the coroutine's output pipe. Output:
   a standalone binary you can `curl` against and get a Response
   back. This is the "FaaS works end to end" milestone.
7. **Transpiler recognition of the fetch-handler idiom** — the
   transpiler learns to lower `export default { async fetch(request)
   { ... } }` and `addEventListener("fetch", ...)` to the
   `faas_fetch_handler_fn` C signature. Test: a JS source becomes
   a working binary from step 6.
8. **Outbound `fetch()` transport** — resolve the Option A vs. B
   question, then implement. At this point OAuth / JWKS / upstream
   calls become expressible. Likely wants a second hello-world-ish
   demo (a hosted function that reverse-proxies).
9. **Redirect handling + bounded hop count.**
10. **Streaming verbs** (`forEachChunk`, `pipeToFile`,
    async-iterator response producers) when the first real hosted
    function asks for them.
11. **Env/ctx bindings** — the Cloudflare-style `fetch(request, env,
    ctx)` second and third arguments. Requires a design pass on what
    *goes in* env (secrets? bindings? config?) so it's not blocking
    the core FaaS shape.
12. **OAuth userland library**, written in JS against the stable
    `fetch` surface. This is the product goal.

## Tradeoffs

- **Header-emit callback inside the parser coroutine**: fast, simple,
  correct. The cost is a coupling surface: `mnvkd`'s parser has to
  know about a callback shape. The callback is intentionally
  domain-free (takes raw bytes), so `jsmx` is not the only possible
  consumer — a future non-JS embedder could write headers into its
  own structure.
- **Slurp-default bodies**: makes the common case fast and simple. The
  cost is that a truly enormous upload (e.g. a 5 GB video) cannot be
  handled in the slurp path and will hit the region's byte budget.
  Such uploads require one of the streaming verbs above, which means
  the transpiler has to recognize them. Hosted functions that want to
  accept huge uploads without that recognition will fail at compile
  time with a boundary report, not at runtime with OOM.
- **No runtime `ReadableStream`**: we give up dynamic stream
  composition in exchange for a dramatically smaller runtime. This
  matches every other jsmx ownership decision (regex, closures,
  `Symbol.iterator`, Promise combinators). Code that needs dynamic
  composition is `manual_runtime_needed`.
- **Cookie jar in userland, not runtime**: keeps `jsmx` free of
  stateful singletons and makes cookie policy explicit. The cost is
  that naïve ports of browser code that relies on the ambient cookie
  jar will not work — they have to use a library.
- **Region-per-request lifetime**: clean billing and memory metering,
  but cross-request state (connection pools, HTTP/2 streams, cached
  TLS sessions) has to live outside the region. That is already the
  shape mnvkd uses, so the cost is bookkeeping, not architecture.

## Status

**Landed** (see [`README.md`](../README.md) and the compliance
fixtures under `compliance/js/wintertc/jsmx/fetch-*`):

- **Fetch JS object model.** `Headers`, `Request`, `Response`, the
  `Body` mixin (`text` / `json` / `arrayBuffer` / `bytes`), and
  `fetch()` as a reject-stub. Seven compliance fixtures cover the
  behavior contract.
- **Fetch transport foundation.** `jsval_body_source_vtable_t`,
  `JSVAL_MICROTASK_KIND_FETCH_BODY_DRAIN`, lazy Request/Response
  bodies, `jsval_request_new_from_parts` /
  `jsval_response_new_from_parts`, `jsval_fetch_emit_header_into`
  reference callback, header-emit typedef for the pending mnvkd
  patch. Six drain scenarios tested in `test_jsval` (text happy
  path, stuttered arrayBuffer, bytes, json, over-budget, mid-stream
  error) plus a `test_codegen` smoke.
- **FaaS handler bridge.** `runtime_modules/shared/faas_bridge.h`
  — header-only glue module matching the `fs_sync.h` /
  `webcrypto_openssl.h` pattern. Defines `faas_fetch_handler_fn`
  (the stable C signature every hand-written or transpiled
  handler must implement), `faas_drain_until_settled` (pumps
  `jsval_microtask_drain` until a Promise settles or the queue
  deadlocks), and `faas_serialize_response` (writes an HTTP/1.1
  status line + headers + authoritative Content-Length + body
  into a caller-supplied byte buffer). Plus jsmx-side helpers:
  `jsval_request_body_snapshot` / `jsval_response_body_snapshot`
  (read-without-consume body accessors) and
  `jsval_request_prewarm_body` (eagerly drains a streaming
  Request body into `body_buffer` without flipping `body_used`,
  so the handler can consume it normally). Nine scenarios tested
  in `test_faas` covering plain and echo handlers, Response.json,
  custom status/headers, Content-Length override, Response.error
  rejection, ENOBUFS buffer-too-small, EDEADLK deadlock detection,
  and streaming-body ENOTSUP.
- **Fetch transport seam + waitlist (slice 8, outbound).**
  `jsval_fetch_transport_t` vtable (`begin`/`poll`/`cancel`) +
  `jsval_region_set_fetch_transport` let an embedder wire a
  `begin`-returns-state / `poll`-returns-READY-or-PENDING-or-ERROR
  transport into the outbound `fetch()` path. A new
  `JSVAL_MICROTASK_KIND_FETCH_REQUEST` pumps the poll via
  `jsval_fetch_run_request`. In parallel, the **fetch waitlist**
  (`jsval_region_set_fetch_waitlist_mode` +
  `jsval_fetch_waitlist_pop` + `jsval_fetch_waitlist_size`) lets
  embedders whose outbound HTTP/1.1 client lives in a coroutine
  body (notably mnvkd's `vk_jsmx_fetch_driver`) park pending
  `(Request, Promise)` pairs in a region-local FIFO and drive
  them from outside a transport callback — the coroutine's
  `vk_read`/`vk_write` macros need to sit at the switch level,
  which a `poll` callback can't satisfy. Six new `test_faas`
  scenarios exercise the transport seam (one-shot, staged
  PENDING→READY, ERROR, no-transport reject-stub regression,
  two-fetch concurrent, waitlist-drives-proxy-handler) against
  an in-process mock transport and the hand-lowered proxy
  handler.
- **Proxy handler lowering recipe (slice 7).**
  `example/wintertc_proxy.js` is the canonical JS source for the
  `export default { async fetch(request) { ... } }` FaaS idiom;
  `example/wintertc_proxy_handler.c` is the hand-lowered output
  the `jsmx-transpile-program` skill now emits for this shape,
  via a CPS transform that registers a continuation through
  `jsval_promise_then`. The skill's `lowering-recipes.md`
  documents the `export default { fetch }` pattern plus the
  `await fetch(...)` → `jsval_fetch` + `jsval_promise_then` CPS
  lowering and the eager-body shortcut for
  `await response.arrayBuffer()`. `test_faas` links the
  hand-lowered handler and runs it against the mock transport
  for both success and upstream-error paths.
- **Cross-repo FaaS proxy demo.** `mnvkd/vk_test_faas_demo.c` +
  `mnvkd/vk_jsmx_fetch.c` run the transpiled proxy handler
  behind mnvkd's coroutine HTTP/1.1 server. The parent responder
  opens an upstream TCP socket via `vk_connect`, spawns
  `vk_jsmx_fetch_driver` as a sub-coroutine bound to that fd,
  waits for its completion, and forwards the parsed upstream
  Response back to the inbound client. End-to-end smoke:
  `bmake vk_test_faas_demo` + `./vk_test_faas_demo &` +
  `nc -q 0 127.0.0.1 8081 <<<$'GET / HTTP/1.1\r\nHost:x\r\nConnection: close\r\n\r\n'`
  returns the hardcoded upstream's real response. The upstream
  base URL is baked in at transpile time (no CLI flag, no
  env var); the transpiled handler *is* the deployment config.

**Not yet implemented:**

- The mnvkd-side header-emit patch to `vk_http11_request()` (the
  inbound integration in the demo prewarms request bodies into
  ArrayBuffers via `faas_build_request_from_parts` instead).
- A mnvkd body-source adapter (`vk_socket` →
  `jsval_body_source_vtable_t`) that reads body bytes lazily
  rather than slurping them up front.
- `https://` outbound (TLS). The demo is plain HTTP; mnvkd's
  TLS support is a follow-on slice.
- Concurrent outbound fetches in the mnvkd driver. The jsmx
  waitlist supports `Promise.all([fetch(a), fetch(b)])` in test
  scenarios, but the mnvkd driver currently serializes fetches
  per request (one driver sub-coroutine at a time).
- Redirect chains, cookie jars, cache — non-goals per the
  ownership split above.
