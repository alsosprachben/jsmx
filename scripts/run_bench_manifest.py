#!/usr/bin/env python3

import argparse
import json
import os
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path


RUNTIME_SOURCES = [
    "jsmn.c",
    "jsnum.c",
    "jsval.c",
    "jsmethod.c",
    "jsregex.c",
    "jsstr.c",
    "unicode.c",
    "utf8.c",
    "jsurl.c",
]

KNOWN_FEATURES = {
    "regex",
}


def parse_preprocessor_defines():
    defines = {}
    for token in shlex.split(os.environ.get("CFLAGS", "")):
        if not token.startswith("-D"):
            continue
        define = token[2:]
        if "=" in define:
            name, value = define.split("=", 1)
        else:
            name, value = define, "1"
        defines[name] = value
    return defines


def enabled_features():
    features = set()
    env_features = os.environ.get("JSMX_FEATURES", "")
    if env_features:
        features.update(
            feature.strip() for feature in env_features.split(",") if feature.strip()
        )

    defines = parse_preprocessor_defines()
    if defines.get("JSMX_WITH_REGEX") == "1":
        features.add("regex")
    return features


def effective_cflags():
    cflags = shlex.split(os.environ.get("CFLAGS", ""))
    if not any(token.startswith("-O") for token in cflags):
        cflags.append("-O3")
    if "-flto" not in cflags:
        cflags.append("-flto")
    return cflags


def compile_runtime_archive(repo_root, tmpdir, cflags):
    cc = os.environ.get("CC", "cc")
    ar = os.environ.get("AR", "ar")
    objects = []

    for source in RUNTIME_SOURCES:
        source_path = repo_root / source
        obj_path = Path(tmpdir) / (Path(source).stem + ".o")
        cmd = [cc, "-I.", *cflags, "-c", str(source_path), "-o", str(obj_path)]
        proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
        if proc.returncode != 0:
            return None, cmd, proc
        objects.append(obj_path)

    archive = Path(tmpdir) / "libjsmx_bench_runtime.a"
    cmd = [ar, "rcs", str(archive), *(str(obj) for obj in objects)]
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    return archive, cmd, proc


def compile_case(repo_root, tmpdir, case, runtime_archive, cflags):
    cc = os.environ.get("CC", "cc")
    ldflags = shlex.split(os.environ.get("LDFLAGS", ""))
    ldlibs = shlex.split(os.environ.get("LDLIBS", ""))
    fixture = repo_root / case["generated"]
    binary = Path(tmpdir) / case["id"]
    cmd = [cc, "-I.", *cflags, str(fixture), str(runtime_archive), *ldflags, *ldlibs, "-o", str(binary)]
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    return binary, cmd, proc


def run_command(repo_root, cmd):
    started = time.perf_counter_ns()
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    ended = time.perf_counter_ns()
    return proc, (ended - started) / 1_000_000.0


def validate_case(case):
    required = case.get("required_features", [])
    if not isinstance(required, list) or any(feature not in KNOWN_FEATURES for feature in required):
        raise ValueError(
            f"{case.get('id', '?')}: invalid required_features {required!r}"
        )
    if not case.get("expected_stdout"):
        raise ValueError(f"{case.get('id', '?')}: expected_stdout must be non-empty")


def measure_side(repo_root, cmd, expected_stdout, warmup_iterations, measured_iterations, label):
    warmups = []
    iterations = []

    for _ in range(warmup_iterations):
        proc, elapsed_ms = run_command(repo_root, cmd)
        if proc.returncode != 0:
            raise RuntimeError(
                f"{label} warmup failed with exit {proc.returncode}: {proc.stderr.strip()}"
            )
        if proc.stdout != expected_stdout:
            raise RuntimeError(
                f"{label} stdout mismatch during warmup: expected {expected_stdout!r}, got {proc.stdout!r}"
            )
        warmups.append(elapsed_ms)

    for _ in range(measured_iterations):
        proc, elapsed_ms = run_command(repo_root, cmd)
        if proc.returncode != 0:
            raise RuntimeError(
                f"{label} run failed with exit {proc.returncode}: {proc.stderr.strip()}"
            )
        if proc.stdout != expected_stdout:
            raise RuntimeError(
                f"{label} stdout mismatch: expected {expected_stdout!r}, got {proc.stdout!r}"
            )
        iterations.append(elapsed_ms)

    return {
        "command": cmd,
        "stdout": expected_stdout,
        "warmup_iterations": warmup_iterations,
        "measured_iterations": measured_iterations,
        "warmup_ms": warmups,
        "iterations_ms": iterations,
        "total_ms": sum(iterations),
        "mean_ms": statistics.fmean(iterations),
        "min_ms": min(iterations),
        "max_ms": max(iterations),
    }


def manifest_defaults(manifest):
    defaults = manifest.get("defaults", {})
    return {
        "warmup_iterations": int(defaults.get("warmup_iterations", 2)),
        "measured_iterations": int(defaults.get("measured_iterations", 5)),
    }


def env_or_arg(args, env_name, attr_name, default):
    value = getattr(args, attr_name)
    if value is not None:
        return value
    env_value = os.environ.get(env_name)
    if env_value is not None and env_value != "":
        return env_value
    return default


def build_arg_parser():
    parser = argparse.ArgumentParser(description="Run JSMX benchmark manifest against Node and compiled C")
    parser.add_argument("--manifest", default="bench/manifest.json")
    parser.add_argument("--filter")
    parser.add_argument("--iterations", type=int)
    parser.add_argument("--warmup", type=int)
    parser.add_argument("--output")
    parser.add_argument("--node")
    return parser


def main():
    parser = build_arg_parser()
    args = parser.parse_args()
    repo_root = Path(__file__).resolve().parent.parent
    manifest_path = repo_root / args.manifest

    with manifest_path.open("r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    defaults = manifest_defaults(manifest)
    bench_filter = env_or_arg(args, "BENCH_FILTER", "filter", None)
    measured_iterations = int(
        env_or_arg(args, "BENCH_ITERATIONS", "iterations", defaults["measured_iterations"])
    )
    warmup_iterations = int(
        env_or_arg(args, "BENCH_WARMUP", "warmup", defaults["warmup_iterations"])
    )
    output_path = env_or_arg(
        args, "BENCH_OUTPUT", "output", str(repo_root / "bench" / "results" / "latest.json")
    )
    node_cmd = env_or_arg(args, "BENCH_NODE", "node", "node")
    node_path = shutil.which(node_cmd)

    if node_path is None:
        print(f"node executable not found: {node_cmd}", file=sys.stderr)
        return 1

    selected_cases = []
    active_features = enabled_features()
    for case in manifest["cases"]:
        validate_case(case)
        if not set(case.get("required_features", [])) <= active_features:
            continue
        if bench_filter and bench_filter not in case["id"]:
            continue
        selected_cases.append(case)

    if not selected_cases:
        print("no benchmark cases selected", file=sys.stderr)
        return 1

    cflags = effective_cflags()

    with tempfile.TemporaryDirectory(prefix="jsmx-bench-", dir="/tmp") as tmpdir:
        runtime_archive, runtime_cmd, runtime_proc = compile_runtime_archive(repo_root, tmpdir, cflags)
        if runtime_proc.returncode != 0:
            sys.stdout.write(runtime_proc.stdout)
            sys.stderr.write(runtime_proc.stderr)
            print("runtime archive build failed", file=sys.stderr)
            return 1

        node_version = subprocess.run(
            [node_path, "--version"], cwd=repo_root, capture_output=True, text=True
        )
        node_version_text = node_version.stdout.strip() if node_version.returncode == 0 else "unknown"

        report = {
            "schema_version": 1,
            "generated_at": datetime.now(timezone.utc).isoformat(),
            "repo_root": str(repo_root),
            "manifest": str(manifest_path.relative_to(repo_root)),
            "node": {
                "command": node_cmd,
                "resolved_path": node_path,
                "version": node_version_text,
            },
            "build": {
                "cc": os.environ.get("CC", "cc"),
                "ar": os.environ.get("AR", "ar"),
                "cflags": cflags,
                "ldflags": shlex.split(os.environ.get("LDFLAGS", "")),
                "ldlibs": shlex.split(os.environ.get("LDLIBS", "")),
                "runtime_archive_command": runtime_cmd,
            },
            "defaults": {
                "warmup_iterations": warmup_iterations,
                "measured_iterations": measured_iterations,
            },
            "cases": [],
        }

        for case in selected_cases:
            binary, compile_cmd, compile_proc = compile_case(
                repo_root, tmpdir, case, runtime_archive, cflags
            )
            if compile_proc.returncode != 0:
                sys.stdout.write(compile_proc.stdout)
                sys.stderr.write(compile_proc.stderr)
                print(f"benchmark build failed: {case['id']}", file=sys.stderr)
                return 1

            case_warmup = int(case.get("warmup_iterations", warmup_iterations))
            case_measured = int(case.get("measured_iterations", measured_iterations))
            expected_stdout = case["expected_stdout"]
            node_run = measure_side(
                repo_root,
                [node_path, str(repo_root / case["source"])],
                expected_stdout,
                case_warmup,
                case_measured,
                f"node/{case['id']}",
            )
            c_run = measure_side(
                repo_root,
                [str(binary)],
                expected_stdout,
                case_warmup,
                case_measured,
                f"c/{case['id']}",
            )
            ratio = node_run["mean_ms"] / c_run["mean_ms"] if c_run["mean_ms"] else None
            case_report = {
                "id": case["id"],
                "suite": case["suite"],
                "source": case["source"],
                "generated": case["generated"],
                "expected_stdout": expected_stdout,
                "compile_command": compile_cmd,
                "notes": case.get("notes", ""),
                "node": node_run,
                "c": c_run,
                "ratio": {
                    "node_over_c_mean": ratio,
                },
            }
            report["cases"].append(case_report)
            ratio_text = f"{ratio:.3f}x" if ratio is not None else "n/a"
            print(
                f"BENCH {case['id']}: node_mean_ms={node_run['mean_ms']:.3f} "
                f"c_mean_ms={c_run['mean_ms']:.3f} ratio={ratio_text}"
            )

    output_file = Path(output_path)
    if not output_file.is_absolute():
        output_file = repo_root / output_file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with output_file.open("w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=2)
        fh.write("\n")

    print(f"bench cases: {len(report['cases'])}")
    print(f"results written to {output_file}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
