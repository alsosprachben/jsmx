// Repo-authored WinterTC queueMicrotask parity fixture.
//
// queueMicrotask(fn) schedules fn on the microtask queue and
// returns undefined. The callback must be a function with no
// arguments; it runs at the next microtask-drain opportunity.

let ran = 0;
queueMicrotask(() => { ran++; });

// After microtask drain:
// ran === 1
