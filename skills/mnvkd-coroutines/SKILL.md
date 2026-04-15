---
name: mnvkd-coroutines
description: Write a correct mnvkd coroutine on the first try. Use when adding a new server/client/responder in the mnvkd repo or writing a hosted-function runtime that embeds mnvkd. Covers the vk_begin/vk_end state-machine model, the self-struct rule, the I/O macro constraints, error handling, and the parent/child/responder pattern.
---

# mnvkd Coroutines

Use this skill when writing any new `void foo(struct vk_thread *that)`
function in or against [`mnvkd`](../../../../mnvkd). mnvkd coroutines
are *not* regular C functions — they are stackless state machines with
specific rules that are easy to get wrong.

Read these references as needed:

- `references/skeleton.md` — the minimum viable coroutine shape, annotated.
- `references/io-macros.md` — every I/O macro with its yield behavior and argument shape.
- `references/errors-and-cleanup.md` — `vk_raise` / `vk_finally` / `vk_lower` / `vk_perror` patterns.
- `references/parent-child.md` — spawning children, responders, messages, and synchronous request/response.
- `references/pitfalls.md` — the full list of easy-to-get-wrong things, sorted by severity.

Ground truth lives in `../mnvkd/threads.md` (~1065 lines) and
`../mnvkd/vk_thread*.h`. Read those if a detail here contradicts the
repo.

## The mental model

A mnvkd coroutine is a function whose body is actually a giant
`switch (that->counter) { ... }` statement. Every call to a `vk_*`
I/O macro expands into:

1. store a unique `__COUNTER__` value into `that->counter`,
2. `return` from the function (yielding to the scheduler),
3. re-enter the function via a `case __COUNTER__ - 1:` label.

Three consequences follow from that mechanic, and almost every rule
in this skill derives from them:

1. **Stack variables do not survive yields.** Between one `vk_*`
   call and the next, the function returned and was re-entered from
   scratch; any C local is freshly re-allocated. State that needs to
   live across a yield must live in the `self` struct (which is
   allocated off the coroutine's micro-heap and survives re-entry)
   or in fields of `that` itself.
2. **`vk_*` I/O macros can only be invoked from directly inside the
   coroutine body.** The `case` label the macro expands into must
   land inside the outer `switch`. Calling `vk_read` from a nested
   helper function puts the case label in the helper's scope, not
   the coroutine's — it either won't compile or will corrupt the
   state machine. If you need to read bytes inside a helper, either
   inline the helper into the coroutine body, or pre-read the bytes
   in the coroutine body and pass them into the helper as a buffer.
3. **Reentrancy into a coroutine is unsafe.** Signal handlers and
   callbacks that try to yield will find themselves running at a
   state-machine position the switch knows nothing about. Never
   `vk_raise` or call `vk_*` macros from a callback.

Once those three rules are internalized, the rest of mnvkd makes sense.

## Workflow for writing a new coroutine

1. **Decide the shape.** Is it a standalone coroutine (one level deep),
   a parent that spawns children, a child that serves a parent, or a
   responder in a pipeline (like `http11_response`)? The answer
   determines which `vk_*` setup macros you use. See
   `references/parent-child.md`.

2. **Write the skeleton.** Copy from `references/skeleton.md`. Fill in:
   - the function signature (`void my_fn(struct vk_thread *that)`);
   - the `ssize_t rc = 0;` local (required for any I/O macro that
     returns a count);
   - the anonymous `self` struct with *all* state that needs to
     survive a yield;
   - `vk_begin()` at the top;
   - `vk_finally()` / `vk_end()` at the bottom.

3. **Populate `self`.** Anything you would reach for as a C local
   — counters, buffers, parsed values, pointers to malloc'd
   state — goes in `self` instead. C locals are OK for use *between*
   two adjacent `vk_*` calls (inside one switch case), but never
   across them.

4. **Do I/O only in the body.** If you feel a helper function
   coming on, ask whether it yields. If yes, inline it. If no
   (plain C logic, no `vk_*` calls), a helper is fine.

5. **Handle errors via `vk_raise` / `vk_finally`.** Don't sprinkle
   `return` statements throughout the body — they'll leak micro-heap
   allocations. `vk_raise(errno_value)` jumps to `vk_finally()`, which
   is the single cleanup path.

6. **Free what you malloc; don't free `self`.** If you `malloc()`
   buffers inside the coroutine, free them in `vk_finally()`. Never
   call `vk_free()` or `free()` on `self` itself — `vk_end()` handles
   that and double-freeing corrupts the parent's micro-heap.

7. **Verify with `-DDEBUG`.** `vk_dbg` / `vk_dbgf` output is
   filterable via `grep ': LOG '`. If a coroutine prints a log
   before `vk_read` and nothing after, it yielded and never got
   woken — usually a scheduler or pipe-pairing bug.

## The anatomy, at a glance

```c
void my_coroutine(struct vk_thread *that)
{
    ssize_t rc = 0;          /* I/O macros need this name in scope. */

    struct {
        /* Every field in here survives yields. Keep it POD. */
        char line[1024];
        size_t count;
        uint8_t *heap_buf;   /* Pointer is fine; buffer lives in malloc. */
    } *self;

    vk_begin();              /* Allocates self from the micro-heap. */

    /* ---- body: vk_* macros only here ---- */
    for (;;) {
        vk_readline(rc, self->line, sizeof(self->line) - 1);
        if (rc <= 0) { break; }
        vk_write(self->line, rc);
        vk_flush();
        self->count++;
    }

    errno = 0;               /* Clear success path so finally sees 0. */
    vk_finally();            /* Cleanup path (runs always; errno!=0 on error). */
    free(self->heap_buf);    /* OK. */
    if (errno != 0) {
        vk_perror("my_coroutine");
    }

    vk_end();                /* Frees self. Do not free it yourself. */
}
```

## Hard rules (summary)

| Rule | Why |
| --- | --- |
| `vk_*` I/O only in the coroutine body, not in helper functions | state-machine `case` labels must share the switch |
| State that survives yields lives in `self` or `that`, not C locals | C stack is re-allocated on every re-entry |
| `self` must be POD; large buffers go to malloc | `self` is micro-heap-allocated, bounded, stack-ordered |
| Do not manually free `self` in `vk_finally` | `vk_end()` does it; double-free corrupts parent heap |
| Clear `errno` on success before `vk_finally` | otherwise the cleanup block thinks an error occurred |
| Always `vk_flush()` after writes on a completing request | buffered bytes may never hit the wire otherwise |
| Do not yield inside callbacks or signal handlers | the state machine has no case label there |
| Per-request state must be freed inside the for(;;) loop | per-connection state stays in `self`, per-request does not |

## What this skill does NOT cover

- The executor internals (`vk_run`, `vk_kern`, `vk_proc`) — you don't
  need them to write a coroutine.
- `vk_isolate` — process isolation for sandboxed coroutines.
- `vk_server` / `vk_pool` wiring — one-time setup per binary, see
  `vk_test_http11_service.c` for the canonical example.
- Specific protocol helpers (`vk_http11.c`, `vk_fetch.c`,
  `vk_redis.c`) — those are applications *built* with this skill,
  not part of it.

## Reference: canonical examples in the mnvkd tree

- `vk_test_http11_service.c` — minimal HTTP/1.1 server using
  `http11_request` as the top-level handler. 48 lines of `main()`.
- `vk_http11.c:199` — `http11_request`, the request coroutine that
  parses HTTP headers and spawns a responder.
- `vk_http11.c:13` — `http11_response`, the responder coroutine
  that reads the parsed request and writes the response.
- `vk_test_faas_demo.c` (if present) — the cross-repo FaaS demo
  that embeds jsmx, showing how an alternative responder is wired
  in via `vk_http11_set_response_handler()`.

If you're learning by example, read `http11_response` first — it's
shorter and demonstrates the per-connection loop pattern cleanly.
