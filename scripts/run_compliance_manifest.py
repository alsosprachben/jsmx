#!/usr/bin/env python3

import json
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


RUNTIME_SOURCES = [
    "jsmn.c",
    "jsval.c",
    "jsstr.c",
    "unicode.c",
    "utf8.c",
    "mnurl.c",
]


def status_from_exit(contract, code):
    if code == contract["pass_exit_code"]:
        return "pass"
    if code == contract["known_unsupported_exit_code"]:
        return "known_unsupported"
    if code == contract["wrong_result_exit_code"]:
        return "wrong_result"
    return f"unexpected({code})"


def compile_case(repo_root, tmpdir, case):
    cc = os.environ.get("CC", "cc")
    cflags = shlex.split(os.environ.get("CFLAGS", ""))
    ldflags = shlex.split(os.environ.get("LDFLAGS", ""))
    fixture = repo_root / case["generated"]
    binary = Path(tmpdir) / case["id"]
    cmd = [cc, "-g", "-I.", *cflags, str(fixture)]
    cmd.extend(str(repo_root / src) for src in RUNTIME_SOURCES)
    cmd.extend(ldflags)
    cmd.extend(["-o", str(binary)])
    proc = subprocess.run(cmd, cwd=repo_root, capture_output=True, text=True)
    if proc.stdout:
        sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    if proc.returncode != 0:
        print(f"BUILD FAIL {case['suite']}/{case['id']}")
        return None
    return binary


def run_case(repo_root, binary, case, contract):
    proc = subprocess.run([str(binary)], cwd=repo_root, capture_output=True, text=True)
    if proc.stdout:
        sys.stdout.write(proc.stdout)
    if proc.stderr:
        sys.stderr.write(proc.stderr)
    actual = status_from_exit(contract, proc.returncode)
    expected = case["expected_status"]
    if actual != expected:
        print(
            f"STATUS FAIL {case['suite']}/{case['id']}: expected {expected}, got {actual}"
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

    with tempfile.TemporaryDirectory(prefix="jsmx-compliance-", dir="/tmp") as tmpdir:
        for case in cases:
            binary = compile_case(repo_root, tmpdir, case)
            if binary is None:
                return 1
            built += 1
            if not run_case(repo_root, binary, case, contract):
                return 1
            passed += 1

    print(f"compliance cases: {passed}/{built} matched expected status")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
