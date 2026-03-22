#!/bin/bash

# Exit immediately if any command fails
set -e

echo "=== HFT Allocator Benchmark Automation ==="

# 1. Lock the CPU to maximum performance
echo "[1/3] Locking Pi 5 CPU to performance mode..."
echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null

# SAFETY TRAP: Automatically restore the CPU governor when the script exits or is aborted
trap 'echo -e "\nRestoring CPU governor to schedutil..."; echo "schedutil" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null' EXIT

# 2. Recompile the Project
echo "[2/3] Compiling latest changes..."
cd build
make -j4
cd ..

# 3. Execute with Core Isolation
echo "[3/3] Launching isolated benchmark..."
taskset -c 1,2,3 ./build/benchmarks/pool_benchmarks

echo "=== Benchmark Complete ==="