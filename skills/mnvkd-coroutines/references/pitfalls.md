# Pitfalls

Sorted by severity. Read the top three before writing a coroutine;
the rest come up less often but will cost you hours to debug.

## CRITICAL: Calling vk_* I/O from a helper function

```c
/* BROKEN — do not do this */
static void read_line(struct vk_thread *that, char *buf, size_t cap)
{
    ssize_t rc;
    vk_readline(rc, buf, cap);    /* case label is in the wrong function */
}
```

`vk_readline` expands to a `case __COUNTER__ - 1:` label. That label
lives inside `read_line`, but the surrounding `switch` is in the
coroutine that *called* `read_line`. Result: compile error, or
silent state-machine corruption on some toolchains.

**Fix**: inline the I/O into the coroutine body, or pre-fetch bytes
into a buffer in the body and pass the buffer to the helper:

```c
/* In the coroutine body: */
vk_readline(rc, self->line, sizeof(self->line) - 1);
parse_line(self->line, rc);   /* helper takes plain C inputs */
```

Helper functions that do not yield (plain C logic, value building,
string manipulation, calls into libc or jsmx) are fine. The rule
is only about `vk_*` I/O macros.

## CRITICAL: Storing large buffers in self

```c
struct {
    uint8_t huge_buffer[1048576];   /* 1 MiB — overflows the micro-heap */
    jsval_region_t region;
    uint8_t region_storage[262144];
} *self;
```

`self` is allocated from the per-coroutine micro-heap, which is
bounded (typically a few KiB to tens of KiB depending on the
`vk_alloc_size()` budget). Putting a 1 MiB array in `self` will
overflow the heap at `vk_begin()` and corrupt the parent.

**Fix**: keep `self` small (pointers and POD fields only), and
allocate large buffers with `malloc` inside the body:

```c
struct {
    uint8_t *big_buf;         /* pointer — OK */
    size_t big_buf_len;
} *self;

/* In the body: */
self->big_buf = malloc(big_buf_len);
if (self->big_buf == NULL) { vk_raise(ENOMEM); }
/* ... use it ... */

/* In vk_finally: */
free(self->big_buf);
```

## CRITICAL: Stack variables don't survive yields

```c
vk_begin();
char line[256];                   /* stack local */
vk_readline(rc, line, sizeof(line));   /* yields */
printf("%s\n", line);             /* line might be garbage here */
```

When `vk_readline` yielded, the coroutine's function *returned*.
When it was resumed, the function was entered fresh and `line`
was re-allocated (possibly at a different stack address). Any
bytes you thought were in `line` are lost.

**Fix**: put the buffer in `self`:

```c
struct { char line[256]; } *self;
vk_begin();
vk_readline(rc, self->line, sizeof(self->line));
printf("%s\n", self->line);       /* safe */
```

C locals are fine for single-use values between two adjacent
statements within one switch case (`rc`, loop counters, pointers
to `self->` fields). The rule is only about surviving a yield.

## CRITICAL: Manually freeing self

```c
vk_finally();
vk_free();           /* WRONG — double-frees self */
vk_end();
```

`vk_end()` already pops `self` off the micro-heap. Calling
`vk_free()` yourself pops one extra frame, which happens to be the
*parent's* state — you just corrupted the parent's heap.

**Fix**: never call `vk_free()` or `free()` on `self` itself. Free
individual pointer fields *within* `self`, and let `vk_end()` handle
`self` as a unit.

## IMPORTANT: Forgetting vk_flush()

```c
vk_write(self->response, self->response_len);
/* ... end of request ... */
/* Missing: vk_flush() */
```

`vk_write` queues bytes into a ring buffer. Without `vk_flush()`,
those bytes may stay buffered indefinitely — the client never sees
the response. Always `vk_flush()` before ending a response or
closing the write side.

## IMPORTANT: Forgetting to clear errno before vk_finally

```c
/* body completes successfully, errno is 17 from an earlier
 * libc call that returned success but dirtied errno */
vk_finally();
if (errno != 0) {    /* incorrectly runs the error branch */
    vk_perror("handler");
}
```

**Fix**: always `errno = 0;` on the success path immediately before
falling through to `vk_finally`.

## IMPORTANT: Per-request state accumulating across loop iterations

```c
for (;;) {
    vk_read(rc, (char*)&self->request, sizeof(self->request));
    if (rc == 0) break;

    self->body_buf = malloc(self->request.content_length);
    /* ... use it ... */
    /* Missing: free(self->body_buf) before next iteration */
}
```

On the second request, `self->body_buf` is overwritten with a
fresh malloc, leaking the first. On the third request, leaks
again. Fix:

```c
for (;;) {
    /* ... */
    free(self->body_buf);
    self->body_buf = NULL;
}
```

Per-connection state (e.g. the socket) stays in `self` across
iterations. Per-request state must be freed inside the loop.

## MINOR: `vk_raise` from inside a callback

```c
static int my_callback(void *userdata, ...)
{
    if (bad) {
        vk_raise(EINVAL);    /* wrong — no state-machine context */
    }
}
```

Callbacks called from libraries (e.g., a crypto library's error
handler) don't have the coroutine's switch-case context. `vk_raise`
will either not compile or jump somewhere nonsense.

**Fix**: have the callback return a sentinel value. Check the
sentinel in the coroutine body and call `vk_raise` there.

## MINOR: Double vk_flush() at end of responder loop

Not harmful but wasteful. One `vk_flush()` per response iteration
is enough; the final `vk_flush()` after the loop catches any
trailing bytes.

## MINOR: Using `vk_write_literal` with a runtime string

`vk_write_literal("foo")` uses `sizeof("foo") - 1` under the hood.
It only works for compile-time string literals. For runtime
strings, use `vk_write(str, strlen(str))`.

## Debugging tip

Compile with `-DDEBUG` and scatter `vk_dbg("before read")`,
`vk_dbg("after read")` through the body. If you see "before" but
not "after", the coroutine yielded and was never resumed — usually
a pipe pairing bug (the writer you're reading from hasn't produced
anything, or the scheduler isn't running it).

`vk_dbgf("fmt=%s", str)` takes a printf format.

Filter the noisy framework output with:

```
./my_program 2>&1 | grep -E 'my_coroutine|specific message'
```
