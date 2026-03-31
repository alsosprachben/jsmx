#!/usr/bin/env python3

import concurrent.futures
import json
import os
import shlex
import subprocess
import sys
import tempfile
from collections import Counter
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
    "mnurl.c",
]

LOWERING_CLASSES = {
    "static_pass",
    "slow_path_needed",
    "unsupported",
}

TRANSLATION_MODES = {
    "idiomatic_flattened",
    "idiomatic_slow_path",
    "literal",
}

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


def status_from_exit(contract, code):
    if code == contract["pass_exit_code"]:
        return "pass"
    if code == contract["known_unsupported_exit_code"]:
        return "known_unsupported"
    if code == contract["wrong_result_exit_code"]:
        return "wrong_result"
    return f"unexpected({code})"


def emit_proc_output(stdout, stderr):
    if stdout:
        sys.stdout.write(stdout)
    if stderr:
        sys.stderr.write(stderr)


def compile_runtime_archive(repo_root, tmpdir):
    cc = os.environ.get("CC", "cc")
    ar = os.environ.get("AR", "ar")
    cflags = shlex.split(os.environ.get("CFLAGS", ""))
    objects = []

    for source in RUNTIME_SOURCES:
        source_path = repo_root / source
        obj_path = Path(tmpdir) / (Path(source).stem + ".o")
        cmd = [cc, "-g", "-I.", *cflags, "-c", str(source_path), "-o", str(obj_path)]
        proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
        if proc.returncode != 0:
            return None, proc
        objects.append(obj_path)

    archive = Path(tmpdir) / "libjsmx_compliance_runtime.a"
    cmd = [ar, "rcs", str(archive), *(str(obj) for obj in objects)]
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    return archive, proc


def compile_case(repo_root, tmpdir, case, runtime_archive):
    cc = os.environ.get("CC", "cc")
    cflags = shlex.split(os.environ.get("CFLAGS", ""))
    ldflags = shlex.split(os.environ.get("LDFLAGS", ""))
    ldlibs = shlex.split(os.environ.get("LDLIBS", ""))
    fixture = repo_root / case["generated"]
    binary = Path(tmpdir) / case["id"]
    cmd = [cc, "-g", "-I.", *cflags, str(fixture)]
    cmd.append(str(runtime_archive))
    cmd.extend(ldflags)
    cmd.extend(ldlibs)
    cmd.extend(["-o", str(binary)])
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    return binary, proc


def run_case(repo_root, binary):
    return subprocess.run([str(binary)], cwd=repo_root, capture_output=True, text=True)


def execute_case(repo_root, tmpdir, case, contract, runtime_archive):
    binary, build_proc = compile_case(repo_root, tmpdir, case, runtime_archive)
    result = {
        "case": case,
        "build_stdout": build_proc.stdout,
        "build_stderr": build_proc.stderr,
        "build_returncode": build_proc.returncode,
        "run_stdout": "",
        "run_stderr": "",
        "actual": None,
    }
    if build_proc.returncode != 0:
        return result

    run_proc = run_case(repo_root, binary)
    result["run_stdout"] = run_proc.stdout
    result["run_stderr"] = run_proc.stderr
    result["actual"] = status_from_exit(contract, run_proc.returncode)
    return result


def worker_count(case_count):
    value = os.environ.get("COMPLIANCE_JOBS", os.environ.get("JOBS"))
    if value:
        try:
            return max(1, min(case_count, int(value)))
        except ValueError:
            pass
    return max(1, min(case_count, os.cpu_count() or 1))


def report_case_result(result, contract):
    case = result["case"]
    emit_proc_output(result["build_stdout"], result["build_stderr"])
    if result["build_returncode"] != 0:
        print(f"BUILD FAIL {case['suite']}/{case['id']}")
        return False

    emit_proc_output(result["run_stdout"], result["run_stderr"])
    actual = result["actual"]
    expected = case["expected_status"]
    if actual != expected:
        print(
            f"STATUS FAIL {case['suite']}/{case['id']}: expected {expected}, got {actual}"
        )
        return False
    return True


def validate_case(case):
    lowering_class = case.get("lowering_class")
    translation_mode = case.get("translation_mode")
    required_features = case.get("required_features", [])
    if lowering_class not in LOWERING_CLASSES:
        print(
            f"MANIFEST FAIL {case.get('suite', '?')}/{case.get('id', '?')}: "
            f"invalid lowering_class {lowering_class!r}"
        )
        return False
    if translation_mode not in TRANSLATION_MODES:
        print(
            f"MANIFEST FAIL {case.get('suite', '?')}/{case.get('id', '?')}: "
            f"invalid translation_mode {translation_mode!r}"
        )
        return False
    if not isinstance(required_features, list) or any(
        feature not in KNOWN_FEATURES for feature in required_features
    ):
        print(
            f"MANIFEST FAIL {case.get('suite', '?')}/{case.get('id', '?')}: "
            f"invalid required_features {required_features!r}"
        )
        return False
    return True


def main():
    repo_root = Path(__file__).resolve().parent.parent
    manifest_path = repo_root / "compliance" / "manifest.json"

    with manifest_path.open("r", encoding="utf-8") as fh:
        manifest = json.load(fh)

    contract = manifest["runner_contract"]
    cases = manifest["cases"]
    built = 0
    passed = 0
    skipped = 0
    class_counts = Counter()
    mode_counts = Counter()
    active_features = enabled_features()
    jobs = worker_count(len(cases))

    with tempfile.TemporaryDirectory(prefix="jsmx-compliance-", dir="/tmp") as tmpdir:
        for case in cases:
            if not validate_case(case):
                return 1
            class_counts[case["lowering_class"]] += 1
            mode_counts[case["translation_mode"]] += 1

        runtime_archive, runtime_proc = compile_runtime_archive(repo_root, tmpdir)
        emit_proc_output(runtime_proc.stdout, runtime_proc.stderr)
        if runtime_proc.returncode != 0:
            print("RUNTIME BUILD FAIL")
            return 1

        with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as executor:
            futures = [
                executor.submit(
                    execute_case, repo_root, tmpdir, case, contract, runtime_archive
                )
                for case in cases
                if set(case.get("required_features", [])) <= active_features
            ]
            skipped = len(cases) - len(futures)
            for future in futures:
                result = future.result()
                built += 1
                if not report_case_result(result, contract):
                    return 1
                passed += 1

    print(f"compliance cases: {passed}/{built} matched expected status")
    if skipped:
        print(f"skipped cases: {skipped}")
    print(
        "lowering classes: "
        + ", ".join(
            f"{name}={class_counts.get(name, 0)}"
            for name in ("static_pass", "slow_path_needed", "unsupported")
        )
    )
    print(
        "translation modes: "
        + ", ".join(
            f"{name}={mode_counts.get(name, 0)}"
            for name in ("idiomatic_flattened", "idiomatic_slow_path", "literal")
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
