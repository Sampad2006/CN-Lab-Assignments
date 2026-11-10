# Assignment 4: Code Division Multiple Access (CDMA) with Walsh Codes

**Course:** CSE/PC/B/S/314 Computer Networks Lab (CO3/CO4)  
**Topic:** Implementation and Evaluation of CDMA Multiple Access of a Common Channel using Orthogonal Walsh Sets (Hadamard Matrices).  
**Student:** Sampad De (Roll No: 002410501025)  
**Language & Platform:** C++17, POSIX TCP Sockets, Multi-threading, Python 3, macOS / Linux  

---

## 1. Overview & Mathematical Architecture

In Code Division Multiple Access (CDMA), multiple contending network stations share a common physical communication channel concurrently without frequency splitting (FDMA) or time interleaving (TDMA). Every active sender is assigned a unique, mutually orthogonal chip sequence from the **Walsh Set** (generated via Sylvester's construction of Hadamard matrices).

### 1.1 Orthogonal Walsh Sequences ($H_M$)
For $N$ stations, the chip sequence length $M$ is chosen as the smallest power of 2 such that $M \ge N$ ($M = 2^{\lceil \log_2 N \rceil}$). The Walsh matrix $H_M$ of dimension $M \times M$ is constructed recursively:
$$H_1 = [1], \quad H_{2k} = \begin{bmatrix} H_k & H_k \\ H_k & -H_k \end{bmatrix}$$

Each row $i \in \{1, \dots, M\}$ represents the unique orthogonal code $W_i \in \{-1, +1\}^M$ assigned to Station $i$. Mutual orthogonality is strictly preserved:
$$W_i \cdot W_j = \sum_{k=0}^{M-1} W_{ik} W_{jk} = \begin{cases} M, & \text{if } i = j \\ 0, & \text{if } i \ne j \end{cases}$$

### 1.2 Bipolar Data Modulation & Spreading
Data bits sent by station $i$ are mapped into bipolar symbols $d_i$:
* Bit `1` $\implies d_i = +1$
* Bit `0` $\implies d_i = -1$
* Silent / Inactive $\implies d_i = 0$

Station $i$ modulates its symbol across the medium by multiplying with its assigned Walsh vector $W_i$:
$$C_i = d_i \cdot W_i = [d_i W_{i,0}, d_i W_{i,1}, \dots, d_i W_{i,M-1}]$$

### 1.3 Additive Physical Superposition
The common transmission medium acts as a linear algebraic adder, superimposing all transmitted chip vectors:
$$S = \sum_{i=1}^N C_i = \sum_{i=1}^N d_i W_i$$
When simulated channel noise $\eta \sim \mathcal{N}(0, \sigma^2)$ is enabled:
$$S_{\text{noisy}} = S + \eta$$

### 1.4 Perfect Orthogonal Demultiplexing & Reconstruction
Any receiving station $j$ decodes the transmission of station $i$ by computing the normalized inner product (cross-correlation) between the composite channel signal $S$ and station $i$'s Walsh code $W_i$:
$$\frac{1}{M} (S \cdot W_i) = \frac{1}{M} \left( \sum_{k=1}^N d_k W_k \right) \cdot W_i = \frac{1}{M} \sum_{k=1}^N d_k (W_k \cdot W_i)$$
Because $W_k \cdot W_i = 0$ for all $k \ne i$, all cross-interferences cancel out identically to 0:
$$\frac{1}{M} (S \cdot W_i) = \frac{1}{M} (d_i \cdot M) = d_i$$

Decision Rule:
* $\frac{1}{M}(S \cdot W_i) \ge +0.5 \implies$ **Bit 1**
* $\frac{1}{M}(S \cdot W_i) \le -0.5 \implies$ **Bit 0**
* $-0.5 < \frac{1}{M}(S \cdot W_i) < +0.5 \implies$ **Silent / Idle**

---

## 2. Directory Structure

```
a4/
├── Makefile                     # Build system (all, server, station, simulator, test, clean)
├── README.md                    # Project documentation
├── run.sh                       # Interactive runner script
├── EVALUATION_REPORT.md         # Comprehensive lab evaluation & experimental analysis
├── report.tex                   # Publication-grade LaTeX report
├── include/
│   ├── cdma.hpp                 # Channel server & station client controllers
│   ├── frame.hpp                # 12-byte framed packet protocol & serializers
│   ├── socket.hpp               # Cross-platform RAII POSIX TCP socket wrapper
│   └── walsh.hpp                # Walsh matrix generator, math kernel & spreading
├── src/
│   ├── cdma.cpp                 # Channel barrier, session management & reconstruction
│   ├── frame.cpp                # Binary serialization with network endianness
│   ├── server.cpp               # Channel Server entry point (./server)
│   ├── simulator.cpp            # Multi-station automated simulation engine (./simulator)
│   ├── socket.cpp               # TCP socket abstraction layer
│   ├── station.cpp              # Interactive & scripted station client (./station)
│   └── walsh.cpp                # Sylvester generation, inner products & noise kernel
├── tests/
│   └── test_suite.cpp           # Unit tests (Sylvester, orthogonality, spreading, etc.)
└── eval/
    ├── benchmark.py             # Automated scaling & noise benchmark sweeps
    ├── plot_metrics.py          # Vector SVG graph generator
    └── results/                 # Generated CSV datasets & SVG figures
```

---

## 3. Build & Execution Instructions

### Compilation:
```bash
# Build all executables (server, station, simulator, test_suite)
make all

# Run unit tests
make test
```

### Interactive Menu:
```bash
./run.sh
```

### Running Standalone Channel Server & Station Clients:
Open separate terminal windows:

**Terminal 1 (Channel Server, N=4 stations on port 9090):**
```bash
./server -P 9090 -N 4
```

**Terminal 2 (Station 1):**
```bash
./station -H 127.0.0.1 -P 9090
```

**Terminal 3 (Station 2):**
```bash
./station -H 127.0.0.1 -P 9090
```

**Terminal 4 (Station 3):**
```bash
./station -H 127.0.0.1 -P 9090
```

**Terminal 5 (Station 4):**
```bash
./station -H 127.0.0.1 -P 9090
```
Once all $N=4$ stations connect and register, the server synchronizes slotted rounds. In each round, stations prompt for bit transmission (`1`, `0`, or `s`), the channel server computes linear superposition $S$, broadcasts it, and every station decodes both its own and all other stations' transmissions!

### Running High-Speed Autonomous Simulations:
```bash
# Multi-Station Slotted Bit Simulation (N=4 stations, 30 rounds)
./simulator -N 4 -r 30

# Concurrent Multi-Station Text Broadcast over CDMA
./simulator --msg-test

# Bit Error Rate (BER) vs Channel Noise Benchmark
./simulator --ber-test
```

### Running Benchmarks & Plot Generation:
```bash
python3 eval/benchmark.py
python3 eval/plot_metrics.py
```
Generated vector plots and CSV logs are saved directly in `eval/results/`.

---

## 4. Key Differentiators & Enhancements
* **C++17 Object-Oriented Architecture**: Aligns directly with past assignments (`a1-c`, `a2`, `a3`), employing RAII socket management, clean modular namespaces, and rigorous encapsulation.
* **Full Multi-Station Simultaneous Text Message Broadcast**: Beyond manual bit-by-bit entry, stations can transmit arbitrary ASCII text strings simultaneously over the common channel.
* **Spread-Spectrum Noise Robustness**: Incorporates Gaussian channel noise modeling to demonstrate the processing gain ($10 \log_{10} M$) of Walsh orthogonal spreading.
* **Comprehensive Test Suite & Evaluation**: Exhaustive unit testing over all $3^4 = 81$ input permutations, automated performance benchmarking, and pure Python vector SVG plot generation.
