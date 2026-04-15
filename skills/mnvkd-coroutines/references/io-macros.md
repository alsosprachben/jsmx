# I/O macros

All defined in `../mnvkd/vk_thread_io.h`. All require `that` in
lexical scope (they expand using `that` as the implicit coroutine
pointer). All expand into `case __COUNTER__ - 1:` labels, which is
why they cannot be called from helper functions.

## Reads

| Macro | Signature | Yields on | Notes |
| --- | --- | --- | --- |
| `vk_read(rc, buf, len)` | writes up to `len` bytes into `buf`, count in `rc` | input empty | non-greedy — may return fewer bytes than `len`. Loop if you need exactly `len`. |
| `vk_readline(rc, buf, cap)` | read up to `cap` bytes into `buf`, stopping at `\n` or EOF | input empty | returns bytes read in `rc`; trailing `\n` is included. |
| `vk_readrfcline(rc, buf, cap)` | read a CRLF-terminated line | input empty | strips the trailing CRLF; `rc` is the stripped length. Used by HTTP-style line protocols. |
| `vk_readrfcchunkheader(rc, chunkhead)` | read a hex chunk size + optional extensions | input empty | fills `struct vk_rfcchunkhead`. Used by HTTP chunked encoding. |
| `vk_pollread(rc)` | how many bytes are readable without blocking | never | useful for non-blocking polling. |
| `vk_readhup()` | consume the EOF marker sent by the peer | peer not HUP'd | pairs with peer's `vk_hup()`. |
| `vk_pollhup()` | non-blocking: is EOF queued? | never | use as a loop condition. |

## Writes

| Macro | Signature | Yields on | Notes |
| --- | --- | --- | --- |
| `vk_write(buf, len)` | write exactly `len` bytes from `buf` | output full | blocks (yields) until all bytes are queued. |
| `vk_write_literal(str)` | write a string literal | output full | `sizeof(str) - 1` is computed at compile time. |
| `vk_writef(rc, linebuf, linelen, fmt, …)` | snprintf into `linebuf`, then `vk_write` the result | output full | `rc` gets the bytes written or -1 on format error. `linebuf` must be part of `self`. |
| `vk_writerfcchunkheader(rc, chunkhead)` | emit a hex chunk size line | output full | for chunked encoding. |
| `vk_writerfcchunkfooter(rc, chunkhead)` | emit the trailing CRLF | output full | must pair with header. |
| `vk_writerfcchunkend()` | emit the final `0\r\n\r\n` | output full | once per chunked response. |
| `vk_flush()` | block until all buffered writes are delivered | output queued | **call before ending a request**, otherwise bytes may never hit the wire. |

## Splicing (zero-copy forwarding)

| Macro | Signature | Notes |
| --- | --- | --- |
| `vk_forward(rc, len)` | forward up to `len` bytes from input to output | no intermediate buffer. `rc` gets the bytes actually forwarded. Useful for streaming bodies. |

## Shutdown / close

| Macro | Notes |
| --- | --- |
| `vk_hup()` | write an EOF marker; blocks until peer acknowledges. |
| `vk_tx_close()` | close the write side (half-close). |
| `vk_rx_close()` | close the read side. |
| `vk_tx_shutdown()` | graceful write-side shutdown. |
| `vk_rx_shutdown()` | graceful read-side shutdown. |

## Common patterns

### Read-exactly

`vk_read` is non-greedy. To read exactly N bytes, loop:

```c
size_t have = 0;
while (have < self->want) {
    vk_read(rc, self->buf + have, self->want - have);
    if (rc <= 0) { vk_raise(EPIPE); }
    have += (size_t)rc;
}
```

### Request/response cycle

```c
vk_readrfcline(rc, self->line, sizeof(self->line) - 1);   /* read request */
/* ... parse ... */
vk_write_literal("HTTP/1.1 200 OK\r\n");
vk_writef(rc, self->line, sizeof(self->line) - 1,
          "Content-Length: %zu\r\n", body_len);
vk_write_literal("\r\n");
vk_write(self->body, body_len);
vk_flush();
```

### Forward a known-length body

```c
vk_forward(rc, self->request.content_length);
if ((size_t)rc < self->request.content_length) {
    vk_raise(EPIPE);
}
```

## Invariants

- **Every I/O macro may yield.** Assume C locals are stale after any
  `vk_*` I/O call. Use `self->` for state that needs to survive.
- **All I/O macros are line-scoped to the coroutine body.** Helpers
  must not call them.
- **`vk_flush()` is not automatic.** Many buffered-write bugs come
  from forgetting it.
