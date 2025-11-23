#!/usr/bin/env python3
"""Run the Black-Scholes benchmark and enforce latency/PMU budgets."""
from __future__ import annotations

import argparse
import json
import os
import pathlib
import subprocess
import sys
from typing import Any, Dict


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=pathlib.Path, required=False,
                        help="CMake build directory that contains the benchmark target")
    parser.add_argument("--bench-exe", type=pathlib.Path, required=False,
                        help="Explicit path to the bs_benchmark executable")
    parser.add_argument("--batch", type=int, default=128,
                        help="Batch size to pass to the benchmark")
    parser.add_argument("--samples", type=int, default=20000,
                        help="Number of measured batches")
    parser.add_argument("--warmup", type=int, default=2000,
                        help="Number of warmup iterations")
    parser.add_argument("--cpu-ghz", type=float, default=3.5,
                        help="Clock speed hint for rdtsc-to-ns conversion")
    parser.add_argument("--budget-mean-ns", type=float, default=900.0,
                        help="Maximum allowed mean ns/price")
    parser.add_argument("--budget-p99-ns", type=float, default=1100.0,
                        help="Maximum allowed p99 ns/price")
    parser.add_argument("--require-pmu", action="store_true",
                        help="Fail if PMU counters are unavailable")
    parser.add_argument("--extra-args", nargs=argparse.REMAINDER,
                        help="Extra passthrough args for the benchmark")
    return parser.parse_args()


def resolve_bench_exe(args: argparse.Namespace) -> pathlib.Path:
    if args.bench_exe:
        return args.bench_exe
    if not args.build_dir:
        raise SystemExit("Either --bench-exe or --build-dir must be provided")
    suffix = "bs_benchmark.exe" if os.name == "nt" else "bs_benchmark"
    candidates = [
        args.build_dir / "benchmarks" / suffix,
        args.build_dir / "bin" / suffix,
        args.build_dir / suffix,
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    raise SystemExit("Benchmark executable not found in build directory; looked at: "
                     + ", ".join(str(c) for c in candidates))


def run_benchmark(exe: pathlib.Path, args: argparse.Namespace) -> Dict[str, Any]:
    cmd = [str(exe), f"--batch={args.batch}", f"--samples={args.samples}", f"--warmup={args.warmup}", f"--cpu-ghz={args.cpu_ghz}"]
    if args.extra_args:
        cmd.extend(args.extra_args)
    result = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        sys.stderr.write(result.stdout)
        sys.stderr.write(result.stderr)
        raise SystemExit(result.returncode)
    output = result.stdout.strip()
    try:
        return json.loads(output)
    except json.JSONDecodeError as exc:  # pragma: no cover - debugging helper
        sys.stderr.write("Failed to parse benchmark JSON. Raw output:\n")
        sys.stderr.write(output + "\n")
        raise SystemExit(f"JSON decode error: {exc}")


def enforce_budgets(report: Dict[str, Any], mean_budget: float, p99_budget: float, require_pmu: bool) -> None:
    measured = report.get("measured_ns_per_price", {})
    mean_ns = measured.get("mean")
    p99_ns = measured.get("p99")
    if mean_ns is None or p99_ns is None:
        raise SystemExit("Benchmark JSON missing measured_ns_per_price metrics")
    violations = []
    if mean_ns > mean_budget:
        violations.append(f"mean ns/price {mean_ns:.2f} exceeds budget {mean_budget:.2f}")
    if p99_ns > p99_budget:
        violations.append(f"p99 ns/price {p99_ns:.2f} exceeds budget {p99_budget:.2f}")

    pmu = report.get("pmu", {})
    if require_pmu:
        if not pmu.get("available"):
            violations.append("PMU counters unavailable (require-pmu enabled)")
        if pmu.get("cycles", 0) <= 0 or pmu.get("instructions", 0) <= 0:
            violations.append("PMU counters did not record positive cycles/instructions")

    if violations:
        raise SystemExit("; ".join(violations))

    summary = {
        "batch": report.get("batch_size"),
        "samples": report.get("samples"),
        "mean_ns": mean_ns,
        "p99_ns": p99_ns,
        "pmu_available": pmu.get("available", False),
        "pmu_cycles": pmu.get("cycles", 0),
        "pmu_instructions": pmu.get("instructions", 0)
    }
    print(json.dumps(summary, indent=2))


def main() -> None:
    args = parse_args()
    exe = resolve_bench_exe(args)
    report = run_benchmark(exe, args)
    enforce_budgets(report, args.budget_mean_ns, args.budget_p99_ns, args.require_pmu)


if __name__ == "__main__":
    main()
