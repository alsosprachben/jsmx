# Runtime Surface

Use this reference to decide whether a JS entrypoint can become a production C program today.

Reusable host/module bridges for production programs live under
`runtime_modules/`, separate from core `jsmx` semantics and separate from
`example/`.

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
  - `undefined`, `null`, booleans, numbers, strings, symbols, bigints, and
    dates
  - native Promise values with explicit runtime-owned microtask scheduling and
    `then` / `catch` / `finally` chaining
  - optional feature-gated WinterTC sync-crypto values:
    - `ArrayBuffer`
    - typed arrays
    - `Crypto`
    - `SubtleCrypto`, including Promise-backed `digest(...)`, HMAC
      `generateKey(...)`, `importKey(...)`, `exportKey(...)`, `sign(...)`,
      and `verify(...)`, AES-GCM `generateKey(...)`, `importKey(...)`,
      `exportKey(...)`, `encrypt(...)`, and `decrypt(...)`, AES-CTR
      `generateKey(...)`, `importKey(...)`, `exportKey(...)`,
      `encrypt(...)`, and `decrypt(...)`, AES-CBC `generateKey(...)`,
      `importKey(...)`, `exportKey(...)`, `encrypt(...)`, and
      `decrypt(...)` (with PKCS#7 padding), AES-KW `generateKey(...)`,
      `importKey(...)`, and `exportKey(...)`, `subtle.wrapKey(...)` and
      `subtle.unwrapKey(...)` dispatching on either an AES-KW or
      AES-GCM wrapping key over the `raw` and `jwk` formats, plus PBKDF2 `importKey("raw", ...)`,
      `deriveBits(...)`, and `deriveKey(...)` to HMAC, AES-GCM, AES-CTR,
      or AES-CBC, plus HKDF `importKey("raw", ...)`, `deriveBits(...)`,
      and `deriveKey(...)` to HMAC, AES-GCM, AES-CTR, or AES-CBC,
      plus ECDSA P-256 `generateKey(...)` returning a
      `{publicKey, privateKey}` pair, `importKey(...)` /
      `exportKey(...)` over `raw` (SEC1 uncompressed public key) and
      `jwk`, and `sign(...)` / `verify(...)` with SHA-256/384/512
      hashes emitting and consuming IEEE P1363 fixed-width `r||s`
      signatures, plus RSASSA-PKCS1-v1_5 `generateKey(...)` returning
      a `{publicKey, privateKey}` pair, `importKey(...)` /
      `exportKey(...)` at 2048/3072/4096-bit modulus
      lengths with public exponent 65537, and `sign(...)` /
      `verify(...)` with SHA-256/384/512 hashes, plus RSA-PSS with
      the same key/modulus/hash policy and per-operation `saltLength`
      (default: digest output length), plus Ed25519 (RFC 8032
      PureEdDSA) `generateKey(...)` / `importKey(...)` /
      `exportKey(...)` / `sign(...)` / `verify(...)` — parameterless,
      deterministic 64-byte signatures, 32-byte keys. The
      asymmetric algorithms support `jwk`, `spki` (public DER),
      and `pkcs8` (private DER) key transport formats; Ed25519
      additionally supports `raw` (32-byte public)
    - `CryptoKey` values for the current HMAC, AES-GCM, AES-CTR, AES-CBC,
      AES-KW, PBKDF2, and HKDF secret-key surfaces over `raw` and `jwk`
      where applicable, plus ECDSA P-256, RSASSA-PKCS1-v1_5,
      RSA-PSS, and Ed25519 asymmetric public/private `CryptoKey`
      values and `CryptoKeyPair` objects returned from `generateKey`
  - optional feature-gated WHATWG Fetch JS object model values
    (full design in [`docs/fetch.md`](../../../docs/fetch.md)):
    - `Headers` (case-insensitive append/set/get/has/delete,
      combine-with-comma reads, value normalization, getSetCookie)
    - `Request` (URL-string or Request input plus init dict with
      method/headers/body; forbidden CONNECT/TRACE/TRACK methods
      rejected; body captured as an in-memory byte snapshot)
    - `Response` (body + status + statusText + headers, plus static
      `Response.error(...)`, `Response.redirect(url, status)` restricted
      to 301/302/303/307/308, and `Response.json(data, init?)` which
      sets `Content-Type: application/json`)
    - `Body` mixin `text(...)`, `json(...)`, `arrayBuffer(...)`, and
      `bytes(...)` returning Promises that resolve synchronously against
      the in-memory body; `bodyUsed` locks after first consumption
    - `fetch(input, init?)` as a value-constructor stub: it parses
      input/init into a `Request` and returns a Promise that rejects
      with `DOMException{name: "TypeError", message: "network not
      implemented yet"}` until the HTTP/1.1 serializer, response parser,
      and mnvkd byte-pipe transport slices land
  - native static function values over translator-emitted call targets
  - native and JSON-backed objects and arrays
  - native `Set` and `Map` values
  - explicit iterator values over strings, arrays, Sets, Maps, and
    feature-gated regex matchAll iterators through the generic stepping helper
  - dense arrays only
  - feature-gated regex values and match iterators
- value operations:
  - truthiness
  - strict and abstract equality
  - number arithmetic, integer coercion, bitwise ops, and shifts
  - explicit bigint construction plus bigint `+`, `-`, `*`, unary `-`,
    equality, and relational comparison
  - explicit mixed Number/BigInt rejection
  - relational comparison
  - `typeof`
  - nullish detection and explicit `??` / ternary lowering patterns
  - native `Date` construction/value helpers through:
    - `jsval_date_new_now(...)`
    - `jsval_date_new_time(...)`
    - `jsval_date_new_iso(...)`
    - `jsval_date_new_local_fields(...)`
    - `jsval_date_new_utc_fields(...)`
    - `jsval_date_now(...)`
    - `jsval_date_utc(...)`
    - `jsval_date_parse_iso(...)`
    - UTC and local field getter/setter helpers
    - `toISOString`, `toUTCString`, `toString`, and `toJSON`
  - explicit Promise helpers through:
    - `jsval_promise_new(...)`
    - `jsval_promise_resolve(...)`
    - `jsval_promise_reject(...)`
    - `jsval_promise_then(...)`
    - `jsval_promise_catch(...)`
    - `jsval_promise_finally(...)`
    - `jsval_promise_all(...)`
    - `jsval_promise_race(...)`
    - `jsval_promise_all_settled(...)`
    - `jsval_promise_any(...)`
    - `jsval_aggregate_error_new(...)`
    - `jsval_dom_exception_errors(...)`
    - `jsval_text_encode_utf8(...)` — TextEncoder.encode equivalent
    - `jsval_text_decode_utf8(...)` — TextDecoder.decode equivalent
    - `jsval_base64_encode(...)` — standard base64 encode (btoa-shaped)
    - `jsval_base64_decode(...)` — standard base64 decode (atob-shaped)
    - `jsval_base64url_encode(...)` — base64url encode (JWT/JWK-shaped)
    - `jsval_base64url_decode(...)` — base64url decode (JWT/JWK-shaped)
    - `jsval_microtask_enqueue(...)`
    - `jsval_queue_microtask(...)` — queueMicrotask(callback) equivalent
    - `jsval_microtask_drain(...)`
    - `jsval_microtask_pending(...)`
    - `jsval_region_set_scheduler(...)`
    - `jsval_region_get_scheduler(...)`
  - optional feature-gated WinterTC crypto through:
    - `jsval_crypto_new(...)`
    - `jsval_crypto_subtle(...)`
    - `jsval_subtle_crypto_digest(...)`
    - `jsval_subtle_crypto_generate_key(...)`
    - `jsval_subtle_crypto_import_key(...)`
    - `jsval_subtle_crypto_export_key(...)`
    - `jsval_subtle_crypto_encrypt(...)`
    - `jsval_subtle_crypto_decrypt(...)`
    - `jsval_subtle_crypto_sign(...)`
    - `jsval_subtle_crypto_verify(...)`
    - `jsval_subtle_crypto_derive_bits(...)`
    - `jsval_subtle_crypto_derive_key(...)`
    - `jsval_crypto_random_uuid(...)`
    - `jsval_crypto_get_random_values(...)`
    - typed-array / buffer helpers used by `getRandomValues(...)`, digest,
      HMAC, AES-GCM, AES-CTR, AES-CBC, AES-KW, PBKDF2, HKDF, ECDSA,
      RSASSA-PKCS1-v1_5, RSA-PSS, and Ed25519 `BufferSource` inputs
  - optional feature-gated WHATWG Fetch JS object model through:
    - `jsval_headers_new(...)`, `jsval_headers_new_from_init(...)`
    - `jsval_headers_append(...)`, `jsval_headers_set(...)`,
      `jsval_headers_delete(...)`, `jsval_headers_has(...)`,
      `jsval_headers_get(...)`, `jsval_headers_get_set_cookie(...)`,
      `jsval_headers_size(...)`, `jsval_headers_entry_at(...)`
    - `jsval_request_new(...)` plus `jsval_request_method(...)`,
      `jsval_request_url(...)`, `jsval_request_headers(...)`,
      `jsval_request_body_used(...)`, `jsval_request_clone(...)`,
      `jsval_request_text(...)`, `jsval_request_json(...)`,
      `jsval_request_array_buffer(...)`, `jsval_request_bytes(...)`
    - `jsval_response_new(...)`, `jsval_response_error(...)`,
      `jsval_response_redirect(...)`, `jsval_response_json(...)`,
      plus `jsval_response_status(...)`, `jsval_response_status_text(...)`,
      `jsval_response_ok(...)`, `jsval_response_type(...)`,
      `jsval_response_redirected(...)`, `jsval_response_url(...)`,
      `jsval_response_headers(...)`, `jsval_response_body_used(...)`,
      `jsval_response_clone(...)`, `jsval_response_text(...)`,
      `jsval_response_json_body(...)`,
      `jsval_response_array_buffer(...)`, `jsval_response_bytes(...)`
    - `jsval_fetch(...)` — parses input/init into a Request and returns
      a Promise rejected with TypeError until the transport slice lands
    - `jsval_request_body_snapshot(...)`,
      `jsval_response_body_snapshot(...)` — read-without-consume body
      accessors used by the FaaS serializer and future outbound fetch
    - `jsval_request_prewarm_body(...)` — eagerly drains a streaming
      Request body into `body_buffer` without flipping `body_used`,
      so the handler can consume it synchronously after the embedder
      pre-drains
  - FaaS handler contract (`runtime_modules/shared/faas_bridge.h`):
    - `faas_fetch_handler_fn` — the stable C signature every hosted
      function must match; a transpiled `export default { async
      fetch(request) { ... } }` lowers to this shape
    - `faas_drain_until_settled(...)` — pumps `jsval_microtask_drain`
      until a Promise settles or the queue deadlocks
    - `faas_serialize_response(...)` — writes an HTTP/1.1 response
      byte stream from a jsval Response into a caller-supplied
      buffer, authoritative Content-Length, skips any caller-set
      Content-Length / Transfer-Encoding
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
  - symbol-keyed own-property get/set/delete on native objects
  - own-property copy and clone
  - dense-array push/pop/shift/unshift/clone/splice
  - dense spread-style assembly already modeled in the corpus
  - capacity-bounded `Set` construction and membership mutation through
    `size` / `has` / `add` / `delete` / `clear`
  - capacity-bounded `Map` construction and ordered membership mutation
    through `size` / `has` / `get` / `set` / `delete` / `clear`
  - explicit iterator production and stepping through:
    - `jsval_get_iterator(...)`
    - `jsval_iterator_next(...)`
    - `jsval_iterator_next_entry(...)`
  - translator-owned `for...of` lowerings over strings, native arrays, Sets,
    and Maps
  - translator-owned `new Set(iterable)` lowerings over strings, arrays, and
    Sets, plus `new Map(iterable)` lowerings when the iterable stays inside
    the supported native entry-iterator surface
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
  - `eval`
- capture-free function values can also lower directly when the translator:
  - emits a static C call target
  - materializes a first-class value through `jsval_function_new(...)`
  - routes calls through `jsval_function_call(...)`

If the JS function behavior depends on those missing categories, classify the program as `manual_runtime_needed`.

## Clear Slow-Path Cases

Use `production_slow_path` only when the contract is already clear. The main allowed cases are:

- host glue outside `jsmx`:
  - CLI argument decoding
  - stdin/stdout/stderr bridges
  - exit-code handling
  - environment and working-directory access through libc
  - synchronous UTF-8 text file reads and writes through libc
  - synchronous directory enumeration and file-type inspection through
    `opendir` / `readdir` / `lstat` / `stat`
  - `realpath`-based visited tracking for recursive CLI tools
  - reusable host-module wrappers surfaced through `runtime_modules/` runtime
    profiles when the bridge contract is already clear
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
- async or event-loop behavior beyond the explicit Promise core:
  - host event-loop ownership
  - timers
  - streams with async callbacks
  - networking
- unsupported JS categories:
  - prototype-chain-sensitive behavior
  - descriptors or accessors
  - sparse arrays or holes
  - JS-visible `BigInt()` constructor semantics
  - bigint division, remainder, bitwise, or shift operators
  - broad legacy Date-string parsing or host-independent timezone policy
    beyond the bounded ISO + libc-local Date helpers
  - `SubtleCrypto` methods beyond `digest(...)`, HMAC
    `generateKey(...)` / `importKey(...)` / `exportKey(...)` /
    `sign(...)` / `verify(...)`, AES-GCM `generateKey(...)` /
    `importKey(...)` / `exportKey(...)` / `encrypt(...)` / `decrypt(...)`,
    AES-CTR `generateKey(...)` / `importKey(...)` / `exportKey(...)` /
    `encrypt(...)` / `decrypt(...)`, AES-CBC `generateKey(...)` /
    `importKey(...)` / `exportKey(...)` / `encrypt(...)` / `decrypt(...)`,
    AES-KW `generateKey(...)` / `importKey(...)` / `exportKey(...)`,
    `subtle.wrapKey(...)` / `subtle.unwrapKey(...)` dispatching on
    either an AES-KW or AES-GCM wrapping key over the `raw` and `jwk`
    formats, plus PBKDF2
    `importKey("raw", ...)` / `deriveBits(...)` / `deriveKey(...)` to
    HMAC, AES-GCM, AES-CTR, or AES-CBC, HKDF
    `importKey("raw", ...)` / `deriveBits(...)` / `deriveKey(...)` to
    HMAC, AES-GCM, AES-CTR, or AES-CBC, ECDSA P-256
    `generateKey(...)` / `importKey(...)` / `exportKey(...)` /
    `sign(...)` / `verify(...)` over `raw` (SEC1 uncompressed public
    only) and `jwk` key formats with SHA-256/384/512 hashes, and
    RSASSA-PKCS1-v1_5 `generateKey(...)` / `importKey(...)` /
    `exportKey(...)` / `sign(...)` / `verify(...)` at 2048/3072/4096-bit
    modulus lengths with public exponent 65537 and SHA-256/384/512
    hashes, RSA-PSS over the same key/modulus/hash policy with
    per-operation `saltLength` (default: digest output length), and
    Ed25519 (RFC 8032 PureEdDSA) with deterministic 64-byte
    signatures. All four asymmetric algorithms accept `jwk`, `spki`,
    and `pkcs8` formats for `importKey`/`exportKey`; Ed25519 also
    accepts `raw`. Other wrapping algorithms for
    `wrapKey`/`unwrapKey` (RSA-OAEP), other ECDSA curves (P-384,
    P-521), and the remaining asymmetric algorithms (RSA-OAEP,
    ECDH, X25519) are still pending.
  - JS-visible global / prototype Promise surface beyond the explicit helper
    contract, including arbitrary thenable duck typing and Promise
    combinators such as `all`, `race`, `any`, and `allSettled`
  - JS-visible `Symbol.iterator` / `String.prototype[Symbol.iterator]`
    protocol semantics and iterable duck typing
  - functions as values with closure semantics, dynamic `this`, `arguments`,
    constructor behavior, or `bind` / `call` / `apply`
- WHATWG Fetch surfaces beyond the current object-model slice:
  - real HTTP/1.1 request serializer and response parser
  - the mnvkd byte-pipe transport bridge
    (`connect` / `read` / `write` / `close`)
  - `fetch()` actually issuing a network request (today `fetch()`
    always rejects with `TypeError("network not implemented yet")`)
  - streaming request/response bodies (`ReadableStream`,
    `WritableStream`), `Blob`, `File`, `FormData`, `body.blob()`,
    `body.formData()`
  - `AbortController` / `AbortSignal`
  - redirect-chain handling, cookie jar / `Set-Cookie` parsing, CORS
    preflight, and per-header guard enforcement against the
    `vk_header_list` policy flags
- host contracts with no clear current mapping:
  - exact `console.log` object formatting like Node `util.inspect`
  - arbitrary core-module behavior beyond the documented host bridges

When in doubt, stop with a boundary report rather than silently approximating the missing runtime.
