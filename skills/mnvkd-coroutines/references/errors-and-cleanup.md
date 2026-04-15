# Errors and cleanup

mnvkd's error handling is a non-local jump from anywhere inside the
body to the `vk_finally()` block. Unlike a C `return`, it preserves
the state-machine counter so `vk_finally` runs in the same coroutine
frame.

## Raising errors

| Macro | Effect |
| --- | --- |
| `vk_raise(code)` | sets `errno = code`, jumps to `vk_finally()`. |
| `vk_error()` | equivalent to `vk_raise(errno)` — raises whatever is already in `errno`. Used after a plain libc call fails. |
| `vk_perror(prefix)` | `perror`-style log; does NOT raise. Call from `vk_finally` to log before cleanup. |

Typical usage:

```c
if (self->request.content_length > FAAS_MAX_BODY_BYTES) {
    vk_raise(E2BIG);   /* jump to vk_finally with errno = E2BIG */
}

rc = some_plain_libc_call();
if (rc < 0) {
    vk_error();         /* propagate whatever errno libc set */
}
```

## The vk_finally block

`vk_finally()` marks the single cleanup path. It runs:

1. **On success**: control falls through from the body into
   `vk_finally`. `errno` should have been cleared to 0 just before,
   otherwise the finally block will think an error occurred.
2. **On error**: `vk_raise()` or `vk_error()` jumped here, and
   `errno` is the error code.

Inside `vk_finally`, distinguish the two cases with `errno`:

```c
errno = 0;
vk_finally();
/* Cleanup code that runs on BOTH paths goes first. */
free(self->big_buf);
self->big_buf = NULL;

if (errno != 0) {
    vk_perror("my_coroutine");
    /* Optional: try to write an error response on the wire. */
    vk_write_literal("HTTP/1.1 500 Internal Server Error\r\n"
                     "Content-Length: 0\r\n\r\n");
    vk_flush();
    vk_tx_close();
}

vk_end();
```

## vk_lower — re-entering the body from finally

`vk_lower()` is an advanced primitive that lets `vk_finally` jump
*back* to the point of the `vk_raise`. Useful for retry loops where
you want cleanup to run, then retry the operation. For most
coroutines you will not need it.

## Caught signals

If `errno == EFAULT` after a vk_raise, the error was raised by a
hardware signal (SIGSEGV, SIGBUS, …) that mnvkd's signal machinery
caught. `vk_sigerror()` logs it with the signal name instead of
`strerror(errno)`:

```c
if (errno != 0) {
    if (errno == EFAULT) {
        vk_sigerror();        /* logs "caught signal 11 (SIGSEGV)" */
    } else {
        vk_perror("operation");
    }
}
```

## Never call vk_free(self)

The one cleanup you must NOT do in `vk_finally` is
`vk_free(self)`. `self` was allocated by `vk_begin()` from the
micro-heap, and `vk_end()` pops it off. Manually freeing `self`
from `vk_finally` pops twice, which destroys the parent's heap
ordering and silently corrupts the parent coroutine.

If you malloc'd pointer fields into `self`, free *those pointers*
in `vk_finally`:

```c
vk_finally();
free(self->body_buf);     /* OK — this is a pointer field, not self itself */
free(self->region_storage);
free(self->out_buf);
if (errno != 0) {
    vk_perror("handler");
}
vk_end();                 /* vk_end frees self for you */
```

## Clearing errno on success

Because `vk_finally` always runs, and always inspects `errno`, you
must clear `errno` on the success path before falling through:

```c
/* ... body completed successfully ... */
errno = 0;
vk_finally();
/* ... */
```

Without that clearing, a previous libc call that set `errno` (even
if it returned successfully — some libc calls set `errno` without
signalling error) will trigger the error branch unexpectedly.

## Quick reference

| Situation | Do |
| --- | --- |
| Detect an invalid request | `vk_raise(EINVAL)` |
| Propagate a libc error | `vk_error()` right after the failing call |
| Clean up a malloc buffer | `free(self->ptr)` in `vk_finally` |
| Log and continue on error | `vk_perror("label")` in `vk_finally` |
| Write an error response | `vk_write_literal(...); vk_flush(); vk_tx_close();` in the `if (errno != 0)` branch |
| Free `self` | **don't** — `vk_end()` owns it |
| Reset errno before successful finally | `errno = 0;` immediately before `vk_finally()` |
