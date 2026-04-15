# Coroutine skeleton

The minimum viable mnvkd coroutine. Copy this and fill in the body.

```c
#include "vk_thread.h"
#include "vk_thread_cr.h"    /* vk_begin / vk_end / vk_finally */
#include "vk_thread_io.h"    /* vk_read / vk_write / vk_flush etc. */
#include "vk_thread_err.h"   /* vk_raise / vk_error / vk_perror */

void my_coroutine(struct vk_thread *that)
{
    ssize_t rc = 0;      /* REQUIRED at function scope — I/O macros need this name */

    struct {
        /* Anything you'd normally put in a C local goes here instead.
         * Stack locals do not survive yields; self fields do. Keep it POD:
         * primitive types, arrays, and pointers to malloc'd buffers are fine.
         * Do not put large buffers here directly — self is allocated from
         * the per-coroutine micro-heap, which is bounded. */
        char line_buf[1024];
        size_t byte_count;
        uint8_t *big_buf;    /* pointer — malloc separately, free in vk_finally */
    } *self;                  /* Note: `*self`, allocated by vk_begin() */

    vk_begin();
    /* Between here and vk_finally(), only the coroutine body is safe for
     * vk_* I/O macros. C helper functions may be called, but those helpers
     * MUST NOT invoke any vk_* I/O macro themselves. */

    self->byte_count = 0;
    self->big_buf = NULL;

    /* ... your body ...
     *
     * Typical per-connection loop shape:
     *
     *    for (;;) {
     *        vk_readline(rc, self->line_buf, sizeof(self->line_buf) - 1);
     *        if (rc <= 0) break;
     *        vk_write(self->line_buf, rc);
     *        vk_flush();
     *        self->byte_count += (size_t)rc;
     *    }
     */

    vk_flush();           /* Make sure any buffered writes reach the wire. */
    vk_tx_close();        /* Optional: half-close the write side if you're done. */

    errno = 0;            /* Success path — clear errno so vk_finally skips error code. */
    vk_finally();
    /* vk_finally is the single cleanup path. Runs both on success and on
     * any vk_raise(). Check errno to distinguish. */
    free(self->big_buf);
    self->big_buf = NULL;
    if (errno != 0) {
        vk_perror("my_coroutine");
        /* Optionally: vk_sigerror() if errno == EFAULT from a caught signal. */
    }
    /* DO NOT call vk_free() on self here — vk_end() owns that. */

    vk_end();
}
```

## Why each piece exists

- **`ssize_t rc = 0;`** at function scope: most I/O macros (`vk_read`,
  `vk_write`, `vk_forward`, `vk_readline`, `vk_writef`, …) assign the
  byte count to `rc`. The macros refer to `rc` by name, so it must be
  in scope. A typo like `int rc` works on most platforms but will
  truncate large counts.

- **Anonymous `struct { ... } *self;`** declaration. `vk_begin()`
  allocates one copy of that struct on the coroutine's micro-heap and
  points `self` at it. The struct is anonymous by convention — you
  never need to refer to its type name elsewhere.

- **`vk_begin()` / `vk_end()` bracket.** The body *must* live between
  them. Missing `vk_end()` is a silent bug — the state machine has no
  terminal case and the coroutine will not complete.

- **`vk_finally()`** is a single-exit cleanup path. It runs in both
  the success case (when control falls through from the body) and the
  error case (when `vk_raise(errno_value)` transfers control to it).
  `errno` distinguishes the two.

- **`errno = 0;` before `vk_finally()`** on the success path is
  important. `vk_finally` always inspects `errno`; if a prior I/O call
  happened to leave it nonzero (even on success), your error branch
  will run unexpectedly.

- **Do not `vk_free(self)`**. `vk_end()` pops `self` off the micro-heap
  in the correct order relative to the parent's state. Calling
  `vk_free()` yourself corrupts that ordering and silently destroys
  the parent's heap layout.
