Predict short-horizon order-flow (up/flat/down) and trade only when edge clears costs. Cheap data, lean models, rigorous backtests.

## C++ Verification Tests

Black–Scholes correctness gates now live inside `tests/black_scholes_verification.cpp`. They cover put–call parity, arbitrage bounds, and the extreme limits (T→0 / σ→0) while asserting SIMD batch parity against the scalar path.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON -DUSE_CONAN=OFF
cmake --build build --target black_scholes_verification
ctest --test-dir build --output-on-failure
```

## Latency Benchmark / CI Gate

`benchmarks/bs_benchmark.cpp` emits reproducible JSON with rdtsc/ns stats and PMU counters. CI runs it through `.github/scripts/benchmark_gate.py` and fails when mean ns/price ≥ 900 ns or p99 ≥ 1100 ns.

```bash
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DBUILD_BENCHMARKS=ON -DBUILD_TESTS=OFF -DUSE_CONAN=OFF
cmake --build build-perf --target bs_benchmark
python3 .github/scripts/benchmark_gate.py --build-dir build-perf --batch 128 --samples 20000 --warmup 2000
```
