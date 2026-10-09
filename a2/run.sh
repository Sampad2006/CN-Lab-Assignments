#!/bin/bash
set -e

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
cd "$DIR"

echo "=========================================================="
echo "  Computer Networks Lab: Assignment 2 (Flow Control)      "
echo "  Stop-and-Wait ARQ | Go-Back-N ARQ | Selective Repeat ARQ"
echo "=========================================================="

echo "[1/2] Compiling C++ Executables..."
make clean > /dev/null
make all

echo "[2/2] Build Successful! Generated binaries:"
echo "  - ./sender   (Flow Control Sender CLI)"
echo "  - ./receiver (Flow Control Receiver CLI)"
echo ""
echo "Quick Test Options:"
echo "  1) Run Stop-and-Wait Verification"
echo "  2) Run Go-Back-N Verification (Window N=4)"
echo "  3) Run Selective Repeat Verification (Window N=4)"
echo "  4) Run Full Automated Benchmark Suite (Python)"
echo "  5) Exit"
echo ""
read -p "Select an option [1-5]: " choice

case $choice in
    1)
        echo "Starting Stop-and-Wait test..."
        ./receiver --protocol sw --port 9876 --output tests/received_sw.txt --quiet &
        RECV_PID=$!
        sleep 0.3
        ./sender --protocol sw --file tests/test_input.txt --port 9876 --payload 64
        wait $RECV_PID
        diff -u tests/test_input.txt tests/received_sw.txt && echo ">>> VERIFICATION: SUCCESS (100% BYTE MATCH) <<<"
        ;;
    2)
        echo "Starting Go-Back-N test..."
        ./receiver --protocol gbn --port 9876 --output tests/received_gbn.txt --quiet &
        RECV_PID=$!
        sleep 0.3
        ./sender --protocol gbn --file tests/test_input.txt --port 9876 --window 4 --payload 64
        wait $RECV_PID
        diff -u tests/test_input.txt tests/received_gbn.txt && echo ">>> VERIFICATION: SUCCESS (100% BYTE MATCH) <<<"
        ;;
    3)
        echo "Starting Selective Repeat test..."
        ./receiver --protocol sr --port 9876 --window 4 --output tests/received_sr.txt --quiet &
        RECV_PID=$!
        sleep 0.3
        ./sender --protocol sr --file tests/test_input.txt --port 9876 --window 4 --payload 64
        wait $RECV_PID
        diff -u tests/test_input.txt tests/received_sr.txt && echo ">>> VERIFICATION: SUCCESS (100% BYTE MATCH) <<<"
        ;;
    4)
        echo "Running Full Automated Benchmark Suite..."
        python3 eval/benchmark.py
        ;;
    *)
        echo "Exiting."
        ;;
esac
