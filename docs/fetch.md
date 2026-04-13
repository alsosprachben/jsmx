# Fetch API Design

## Goal

Let hosted JavaScript express `fetch`, `Request`, `Response`, `Headers`, and
the `Body` mixin — enough to run OAuth flows, verify JWTs against an IdP's
JWKS endpoint, and implement billable WinterTC-style hosted functions on top
of mnvkd — without dragging a general-purpose asynchronous runtime into
`jsmx`.

The design splits along a single seam:

- **`jsmx`** owns the WHATWG Fetch *JS object model* and all HTTP-level
  semantics that can be expressed as deterministic data transforms over
  caller-owned memory.
- **`mnvkd`** owns the HTTP/1.1 wire protocol, the coroutine scheduler, and
  every byte that touches a socket.

Neither side has to understand the other's object vocabulary. The two meet at
a small, explicit handoff point described below.

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

- The `vk_http11` iterative HTTP/1.1 parser and its response writer. The
  parser is already coroutine-aware, yields on partial input, and streams
  body bytes through `vk_forward` without copying them into a struct.
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

## Integration seam: the header-emit hook

Today `vk_http11`'s parser copies each parsed header into
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

## The `fetch()` entry point

`fetch(input, init?)` today is a value-constructor stub. Its real shape
after transport integration:

```c
int jsval_fetch(jsval_region_t *region, jsval_t input_value,
        jsval_t init_value, int have_init, jsval_t *promise_ptr);
```

The implementation:

1. Constructs a `Request` from input/init (already shipped).
2. Allocates a fresh `FETCH_REQUEST` microtask that owns:
   - a `vk_socket` pair (outbound TCP, optionally TLS)
   - the request bytes staged in an output ring
   - the Promise to resolve on response
3. The microtask does (in sequence, yielding on each):
   - connect
   - write request line + headers
   - stream the request body (from the in-memory `ArrayBuffer` snapshot)
   - parse response line + headers via `vk_http11_response`, using the
     same header-emit hook to land headers into a `jsval` `Headers`
   - construct a `Response` value with a pending body (body is a
     lazily-drained `ArrayBuffer`, resolved on first `text()` etc.)
   - resolve the outer Promise with the `Response`

The response body is itself drained via a `FETCH_BODY_DRAIN`
microtask when userland consumes it, exactly the same way
inbound bodies work. Inbound and outbound share the same body
drain machinery.

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

1. **mnvkd header-emit hook** — patch `vk_http11` to accept an emit
   callback. Keep the existing `struct request` path as a default
   callback for current callers. No behavior change for mnvkd's own
   tests.
2. **`FETCH_BODY_DRAIN` microtask** — add the new microtask kind plus
   a test-only drain source (e.g. a preloaded byte buffer masquerading
   as a coroutine) so the body-drain behavior can be exercised without
   a real socket.
3. **Inbound request lowering** — a small mnvkd example that accepts
   a request, hands it to a jsmx region as a `Request` value, and
   returns a hand-built `Response`. No userland JS yet; this is the
   "handshake works end to end" slice.
4. **Outbound `fetch()`** — replace the reject-stub with the
   `FETCH_REQUEST` microtask and exercise it against a local test
   server. This unblocks JWKS fetching and OAuth.
5. **Redirect handling + bounded hop count.**
6. **Streaming verbs** (`forEachChunk`, `pipeToFile`,
   async-iterator response producers) when the first real hosted
   function asks for them.
7. **OAuth userland library**, written in JS against the stable
   `fetch` surface.

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

The JS object model is landed and tested: `Headers`, `Request`,
`Response`, `Body` mixin, and `fetch()` as a reject-stub. See
[`README.md`](../README.md) under "Fetch API (object-model slice)"
for the current surface, and the seven compliance fixtures under
`compliance/js/wintertc/jsmx/fetch-*` for the behavior contract.

The mnvkd integration seam (header-emit hook, `FETCH_BODY_DRAIN`
microtask, outbound `fetch()` transport) is the next slice and has
not been implemented.
