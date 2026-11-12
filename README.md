# Computer Networks Laboratory-Sampad De-002410501025 (A1)

**Student Name:** Sampad De  
**Roll Number:** 002410501025  
**Course:** CSE/PC/B/S/314 Computer Networks Laboratory  
**Department:** Department of Computer Science and Engineering  
**Institution:** Jadavpur University, Kolkata  
**Session:** 2026–2027  

---

## 📌 Repository Overview

This repository contains all experimental implementations, simulation models, analytical benchmarks, evaluation reports, and laboratory deliverables developed for the **Computer Networks Laboratory** course at Jadavpur University.

Each assignment is housed in its dedicated folder (`a1/` through `a6/`) and includes full source code, build scripts (`Makefile` / shell runners), compiled PDF reports with academic LaTeX sources (`report.tex`), and supporting figures/visualizations.

---

## 📂 Laboratory Structure & Summary of Assignments

| Directory | Assignment Title | Topics & Methodologies | Deliverables & Artifacts |
| :---: | :--- | :--- | :--- |
| **[`a1/`](./a1/)** | **Error Detection & Correction (CO1)** | Bitwise implementations of **16-bit 1's Complement Checksum** and **Cyclic Redundancy Check (CRC-16 / CRC-32)**; error-injection pipelines with burst/random noise models. | C source code, test evaluation harness, comprehensive PDF report. |
| **[`a2/`](./a2/)** | **Data Link Flow Control (CO2)** | Implementation and benchmark of **Stop-and-Wait ARQ**, **Go-Back-N (GBN)**, and **Selective Repeat (SR)** sliding window protocols with frame timers and ACK/NAK handling. | C/C++ implementations, evaluation scripts, LaTeX report and PDF. |
| **[`a3/`](./a3/)** | **Medium Access Control: CSMA (CO3)** | Discrete-event simulator modeling **1-Persistent**, **Non-Persistent**, and **p-Persistent CSMA**; comparative throughput, collision rate, and channel utilization sweeps. | C++ simulator, Python evaluation scripts, benchmark plots, LaTeX report. |
| **[`a4/`](./a4/)** | **Code Division Multiple Access: CDMA (CO3)** | Orthogonal chip code generation using **Walsh-Hadamard Matrices**; multi-station bipolar signal summation, AWGN channel simulation, and BER vs. noise analysis. | C++ CDMA engine, Walsh matrix generator, benchmarking suite, LaTeX report. |
| **[`a5/`](./a5/)** | **Traffic Dissection & Wireshark Analysis (CO4/CO5)** | Live IEEE 802.11ac packet capture; deep packet inspection (DPI) of Ethernet II, ARP, IPv4/IPv6, ICMP, TCP stream reassembly, HTTP/1.1 latency, and OUI hardware resolution. | Live trace analysis, cropped Wireshark screenshots, comprehensive evaluation report. |
| **[`a6/`](./a6/)** | **Cisco Packet Tracer Network Modeling (CO4/CO5)** | Modeling point-to-point crossover links, hub collision domains, switch CAM/ARP learning, multi-router static routing over serial WAN, DHCP (DORA), DNS, HTTP, and FTP services. | Cisco Packet Tracer models (`.pkt`), high-res lab screenshots, LaTeX report. |
| **[`Group-Assignment/ARP/`](./Group-Assignment/ARP/)** | **Address Resolution Protocol (ARP - RFC 826)** | C++17 implementation of RFC 826 ARP over a Virtual L2 Ethernet Switch (UDP); dynamic/static ARP cache with TTL aging, outbound packet queuing, Gratuitous ARP conflict detection, and ARP spoofing mitigation. | C++17 engine, Virtual Switch daemon, Interactive Host CLI, 7-scenario simulator, unit tests, README. |

---

## 🚀 Environment & Toolchain

* **Operating System:** macOS (Darwin / Apple Silicon)
* **Languages & Compilers:** C (C11), C++ (C++17, `clang++` / `g++`), Python 3.9+
* **Packet Capture & Analysis:** Wireshark v4.x, `libpcap`, `tshark`
* **Network Simulator:** Cisco Packet Tracer v9.0.1
* **Document Preparation:** pdfLaTeX / TeX Live (custom university formatting, TikZ diagrams)

---

## 📖 Building and Compiling Reports

Each assignment containing a LaTeX source can be compiled into a PDF using its local `Makefile`:

```bash
# Navigate to any assignment directory, for example a6:
cd a6

# Build the PDF report using make
make

# Clean auxiliary compilation files
make clean
```

---

## 📜 References & Acknowledgments

* **Project Repository:** [https://github.com/Sampad2006/CN-Lab-Assignments](https://github.com/Sampad2006/CN-Lab-Assignments)
* **Course Outcomes:** CO1 (Error Detection), CO2 (Flow Control), CO3 (MAC & Multiplexing), CO4/CO5 (Packet Sniffing, Routing & Internetworking Protocols).
* All simulations, code implementations, network setups, packet captures, and screenshots were completed and documented by **Sampad De**.
