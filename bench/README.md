# Benchmark Suite

This directory holds a dedicated benchmark corpus for comparing:

- Node.js execution of committed JavaScript benchmark sources
- compiled committed C benchmark programs that model the intended `jsmx`
  lowering

The benchmark suite is separate from `compliance/`:

- compliance is correctness-first
- benchmarks are timing-first

## Layout

- `js/`
  - benchmark source programs executed with Node
- `generated/`
  - committed C benchmark programs compiled against the `jsmx` runtime
- `manifest.json`
  - benchmark-pair metadata
  - expected stdout contract
  - default warmup and measured iteration counts

## Running

Use:

- `make bench`

Optional environment overrides:

- `BENCH_FILTER`
  - substring filter over benchmark ids
- `BENCH_ITERATIONS`
  - measured iterations override
- `BENCH_WARMUP`
  - warmup iterations override
- `BENCH_OUTPUT`
  - JSON output path
- `BENCH_NODE`
  - Node executable path or command name

By default the runner writes JSON results to:

- `bench/results/latest.json`

The C side of the benchmark build defaults to:

- `-O3`
- `-flto`

If `CFLAGS` already supplies an optimization level, the runner keeps that
explicit choice and still adds `-flto` unless it is already present.

## Feature-Gated Cases

- benchmark manifest entries may declare `required_features`
- default `make bench` skips cases whose features are not enabled
- the current corpus includes multiple regex benchmarks gated on `regex`

Run regex-gated benchmarks with the same regex-enabled `CFLAGS`, `LDLIBS`,
and `JSMX_FEATURES=regex` setup used for the regex test matrix.

The regex corpus is split into three lanes:

- end-to-end named-groups `exec` through the high-level `jsval` path
- compile-once repeated `exec` through the lower-level `jsregex` path
- high-level `exec` without `groups` object reads

That path is ignored in Git so local history can accumulate outside the
committed corpus.
