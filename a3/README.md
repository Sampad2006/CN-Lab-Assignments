# Assignment 3: Medium Access Control — CSMA & CSMA/CD Protocols

**Course:** CSE/PC/B/S/314 Computer Networks Lab (CO3)  
**Topic:** Design and Implementation of Medium Access Control (MAC) mechanisms using IEEE 802 standards (CSMA & CSMA/CD).  
**Student:** Sampad De (Roll No: 002410501025)  
**Language & Platform:** C++17, POSIX IPC Sockets, Python 3, macOS / Linux  

---

## 1. Overview & Architecture

This lab project implements and evaluates four fundamental Carrier Sense Multiple Access (CSMA) protocols in a distributed medium access control environment:
1. **Non-Persistent CSMA**: Senses channel. If IDLE, transmits immediately. If BUSY, immediately defers by a random backoff time without continuously monitoring the wire.
2. **1-Persistent CSMA**: Continuously listens to the shared channel while BUSY. As soon as the channel becomes IDLE, transmits with probability $p = 1.0$.
3. **p-Persistent CSMA**: Slotted time model. Waits until IDLE. In each slot, transmits with probability $p$ and defers for one slot ($\tau$) with probability $1-p$.
4. **CSMA/CD (Carrier Sense Multiple Access with Collision Detection)**: Emulates IEEE 802.3 Ethernet MAC behavior. Transmits using 1-persistent sensing, continuously monitors the wire while transmitting, immediately aborts upon detecting a collision, broadcasts a **Jam Signal**, and applies **Truncated Binary Exponential Backoff (BEB)**:
   $$r \sim \text{Uniform}(0, 2^{\min(k, 10)} - 1), \quad T_{\text{backoff}} = r \times \tau$$

### System Components:
* **`server` (`src/server.cpp`, `src/channel.cpp`)**: Standalone channel emulator process managing channel states (`IDLE`, `BUSY`, `COLLISION`), handling carrier sensing queries, logging frame transmissions, detecting overlapping collisions, and tracking total channel utilization.
* **`station` (`src/station.cpp`, `src/csma.cpp`)**: Standalone contending network station process executing the CSMA state machine, managing frame queues, performing carrier sensing, backoff delays, and frame deliveries.
* **`simulator` (`src/simulator.cpp`)**: High-throughput multi-threaded concurrent simulation engine executing large sweeps across station counts $N \in [1, 30]$ and probability parameters $p \in [0.01, 1.0]$ with sub-millisecond precision.
* **`eval/benchmark.py` & `eval/plot_metrics.py`**: Automated test orchestrator and pure Python vector SVG plot generator.

---

## 2. Directory Structure

```
a3/
├── Makefile                     # Build system (all, server, station, simulator, test, clean)
├── README.md                    # Project documentation
├── run.sh                       # Interactive runner menu
├── EVALUATION_REPORT.md         # Formal comprehensive lab evaluation report
├── report.tex                   # Complete LaTeX report
├── include/
│   ├── channel.hpp              # Shared channel state machine & arbitration
│   ├── csma.hpp                 # CSMA protocol configurations & station class
│   ├── frame.hpp                # Message structure, serialization & packet enums
│   └── socket.hpp               # Cross-platform POSIX datagram socket wrapper
├── src/
│   ├── channel.cpp              # Channel logic, active transmissions & statistics
│   ├── csma.cpp                 # Implementation of 4 CSMA algorithms & BEB
│   ├── frame.cpp                # Message encoding / decoding routines
│   ├── server.cpp               # Channel Server entry point
│   ├── simulator.cpp            # Concurrent simulation & sweep binary
│   ├── socket.cpp               # Socket abstraction layer
│   └── station.cpp              # Station client entry point
├── tests/
│   └── test_suite.cpp           # Unit tests for frame encoding, arbitration & IPC
└── eval/
    ├── benchmark.py             # Automated test suite (sweeps p and N)
    ├── plot_metrics.py          # Vector SVG plot generator
    └── results/                 # Generated CSV datasets & SVG figures
```

---

## 3. Build & Execution Instructions

### Compilation:
```bash
# Build all executables (server, station, simulator)
make all

# Run unit tests
make test
```

### Interactive Menu:
```bash
./run.sh
```

### Running Standalone Channel Server & Station Clients:
Open two or more terminal windows:

**Terminal 1 (Channel Server):**
```bash
./server -P 9999
```

**Terminal 2 (Station 1):**
```bash
./station -i 1 -p csma_cd -n 10 -P 9999
```

**Terminal 3 (Station 2):**
```bash
./station -i 2 -p csma_cd -n 10 -P 9999
```

### Running High-Speed Multi-Station Simulations:
```bash
# Non-Persistent CSMA with 5 stations
./simulator -p np -N 5 -n 20

# 1-Persistent CSMA with 5 stations
./simulator -p 1p -N 5 -n 20

# p-Persistent CSMA with 5 stations, p=0.1
./simulator -p pp --prob 0.1 -N 5 -n 20

# CSMA/CD with 10 stations
./simulator -p cd -N 10 -n 20
```

### Running Automated Benchmarks & Plot Generation:
```bash
python3 eval/benchmark.py
python3 eval/plot_metrics.py
```
Generated figures and CSV logs are saved directly in `eval/results/`.

---

## 4. Empirical Evaluation Summary

### Experiment 1: $p$-Persistent CSMA vs. Probability $p$ ($N=5$)
* **Optimal $p^*$:** Peak performance and minimum unnecessary slot idle time occurs near $p^* \approx 0.20$ (matching theoretical $p^* \approx 1/N$).
* **Low $p$ ($p \le 0.05$):** Delays are high ($> 50\text{ ms}$) and throughput is low ($< 40\text{ kbps}$) because stations excessively defer transmission slots even when the channel is free.
* **High $p$ ($p \ge 0.50$):** High contention causes collision rates to spike, increasing transmission retries.

### Experiment 2: Comparative Analysis Across All Schemes vs. Contending Stations $N$
* **Non-Persistent CSMA:** Retains stable throughput across all load levels by deferring immediately when the medium is busy, avoiding massive collision spikes at high $N$.
* **1-Persistent CSMA:** Achieves high throughput under very light load ($N \le 4$), but suffers severe collision breakdown ($> 120$ collisions at $N=30$) under heavy contention as multiple queued stations simultaneously transmit the moment the channel becomes idle.
* **$p$-Persistent CSMA ($p=0.1$):** Drastically suppresses collisions at high $N$ ($9-15$ collisions vs. $126$ in 1-persistent), providing superior stability under congested network traffic.
* **CSMA/CD:** Rapidly detects collisions during frame transmission, broadcasts Jam signals, and executes Binary Exponential Backoff (BEB), maintaining high medium utilization ($> 74\%$) without wasting complete packet transmission times on corrupted frames.
