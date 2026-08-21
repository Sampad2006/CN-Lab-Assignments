#!/bin/bash

# Detect GCC-15 / G++-15 from Homebrew or fallback to system gcc
if command -v gcc-15 &> /dev/null; then
    CC="gcc-15"
elif [ -f "/opt/homebrew/bin/gcc-15" ]; then
    CC="/opt/homebrew/bin/gcc-15"
else
    CC="gcc"
fi

echo "Using Compiler: $($CC --version | head -n 1)"
echo ""

echo "Compiling Receiver..."
$CC src/receiver.c src/info.c src/error_injection.c -I headers -o receiver
if [ $? -ne 0 ]; then
    echo "[ERROR] Receiver compilation failed!"
    exit 1
fi
echo "Receiver compiled successfully!"
echo ""

echo "Compiling Sender..."
$CC src/sender.c src/info.c src/error_injection.c -I headers -o sender
if [ $? -ne 0 ]; then
    echo "[ERROR] Sender compilation failed!"
    exit 1
fi
echo "Sender compiled successfully!"
echo ""

echo "Compiling Test Eval (Benchmark)..."
$CC src/test_eval.c src/info.c src/error_injection.c -I headers -o test_eval -lm
if [ $? -ne 0 ]; then
    echo "[ERROR] Test Eval compilation failed!"
    exit 1
fi
echo "Test Eval compiled successfully!"
echo ""

echo "Build Complete! You can now run ./receiver, ./sender, and ./test_eval"
