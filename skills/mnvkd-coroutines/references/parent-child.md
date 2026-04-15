# Parent / child / responder patterns

mnvkd supports three shapes of coroutine relationships:

1. **Standalone** — a coroutine with no children. Runs, returns.
2. **Parent / child** — a coroutine that spawns one or more children
   and may wait for them. Children inherit the parent's socket.
3. **Responder pipeline** — a parent-child pair where the parent's
   *output pipe* is the child's *input pipe*. The parent writes, the
   child reads. `http11_request` + `http11_response` are the
   canonical example.

## Allocating a child object

```c
struct vk_thread *child_ptr;
vk_calloc_size(child_ptr, 1, vk_alloc_size());
```

`vk_alloc_size()` returns the bytes a single coroutine object needs.
`vk_calloc_size` is a macro over `vk_calloc` that takes (lvalue,
count, size).

## vk_child vs vk_responder

| Macro | Wiring |
| --- | --- |
| `vk_child(child_ptr, fn)` | child inherits the parent's full duplex socket |
| `vk_responder(child_ptr, fn)` | child's input is pinned to the parent's output; child's output is pinned to the parent's ancestor output |

Use `vk_responder` when you want a pipeline: the parent parses
something and writes the parsed form into its own output pipe; the
child reads the parsed form from its input pipe and writes a response
back toward the original client.

## Scheduling the child

```c
vk_calloc_size(self->child_ptr, 1, vk_alloc_size());
vk_responder(self->child_ptr, my_responder);
vk_play(self->child_ptr);          /* add to run queue */
```

`vk_play` enqueues the child but does not wait for it. The child
runs when its input is ready (for a responder) or the next time
the scheduler picks it up.

## Sending messages

There are two message-passing shapes, depending on whether the parent
wants to block on the response.

### Asynchronous send / receive

```c
vk_send(self->child_ptr, &self->send_ft, &self->request_msg);
/* ... parent can do other work here ... */
/* later, child gets the message via vk_recv(): */
```

In the child:

```c
vk_recv(self->recv_msg_ptr);       /* pointer to the message data */
```

### Synchronous request / response

```c
/* Parent side: */
vk_request(self->child_ptr, &self->send_ft, &self->request_msg,
           self->response_ptr);
/* vk_request yields until the child calls vk_respond. */
```

In the child:

```c
vk_listen(self->parent_ft, self->request_ptr);     /* blocks on request */
/* ... process *self->request_ptr ... */
vk_respond(self->parent_ft, &self->response_val);
```

## Waiting for a child to finish

```c
vk_call(self->child_ptr);          /* pause parent, play child */
```

`vk_call` yields the parent until the child runs to `vk_end`.

## The http11_request/http11_response pattern

This is the canonical responder pipeline in mnvkd. The parent
(`http11_request`) parses HTTP headers and for each request writes
a `struct request` into its output pipe. The responder
(`http11_response`) reads the struct from its input pipe and writes
a response back out.

Parent (simplified):

```c
void http11_request(struct vk_thread *that)
{
    ssize_t rc = 0;
    struct {
        struct vk_service service;
        struct vk_future request_ft;
        struct request request;
        struct vk_thread *response_vk_ptr;
    } *self;

    vk_begin();

    vk_calloc_size(self->response_vk_ptr, 1, vk_alloc_size());
    vk_responder(self->response_vk_ptr, http11_response);
    vk_play(self->response_vk_ptr);
    vk_send(self->response_vk_ptr, &self->request_ft, &self->service);

    /* ... parse headers into self->request ... */

    vk_write((char*)&self->request, sizeof(self->request));
    vk_forward(rc, self->request.content_length);  /* stream body */

    vk_finally();
    /* ... */
    vk_end();
}
```

Child:

```c
void http11_response(struct vk_thread *that)
{
    ssize_t rc = 0;
    struct {
        struct vk_service *service_ptr;
        struct request request;
    } *self;

    vk_begin();

    vk_recv(self->service_ptr);

    for (;;) {
        vk_read(rc, (char*)&self->request, sizeof(self->request));
        if (rc == 0) { break; }   /* pipe closed */
        if (rc != sizeof(self->request)) { vk_raise(EPIPE); }

        /* ... read body via vk_read or vk_forward ... */
        /* ... write response ... */
        vk_flush();

        if (self->request.close) { break; }
    }

    vk_tx_close();
    vk_finally();
    vk_end();
}
```

## Allocation ordering is a stack

`vk_calloc` is a bump allocator on a stack-ordered micro-heap.
The parent's allocations must be freed after the child's. In
practice this means:

- Allocate child objects *after* the parent's own state.
- Do NOT call `vk_free()` on `self` inside the child's `vk_finally`
  — the parent frees the child object after `vk_call` returns (if
  the parent called it). If the child is fire-and-forget (via
  `vk_play` + no `vk_call`), mnvkd's scheduler handles it.

## When to use which shape

| Goal | Pattern |
| --- | --- |
| Simple one-shot handler (read, write, done) | Standalone coroutine |
| Parent parses, child emits response in a separate stage | Responder pipeline (`vk_responder` + `vk_play` + `vk_send`) |
| Parent delegates work and needs the result | `vk_request` / `vk_listen` / `vk_respond` |
| Parent launches workers in parallel | Multiple `vk_play` calls, no `vk_call` |
| Parent runs workers sequentially | `vk_call` per child |
