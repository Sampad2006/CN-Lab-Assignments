# Assignment 5: Packet Tracer and Traffic Analysis with Wireshark

This directory contains the formal laboratory report, extracted high-resolution Wireshark artifacts, empirical statistics, and complete question-by-question analysis for **Assignment 5** of the **CSE/PC/B/S/314 Computer Networks Lab**.

## Student Information
* **Name:** Sampad De
* **Roll Number:** 002410501025
* **Department:** Computer Science and Engineering, Jadavpur University
* **Assignment Title:** Packet Tracer and Traffic Analysis with Wireshark
* **Target Web Host:** `http://neverssl.com/index.html`

---

## Directory Contents
* **[REPORT.md](REPORT.md):** Comprehensive, publication-grade Markdown laboratory report with embedded diagrams, packet tables, KaTeX formulas, and exhaustive solutions for all 10 questions.
* **[report.tex](report.tex):** Complete, publication-quality LaTeX source code matching the departmental report guidelines (`Report_guidelines.pdf`). Ready to compile into PDF locally or on Overleaf.
* **[Makefile](Makefile):** Build automation script to compile `report.tex` into `report.pdf`.
* **[images/](images/):** Directory containing all 22 extracted high-resolution Wireshark screenshots and terminal traces, with both original index names (`image1.png`–`image22.png`) and descriptive semantic names (`q1_ping_terminal.png`, etc.).

---

## Summary of Answers for Questions 1–10

| Question | Core Topic | Key Findings & Quantitative Answers |
| :---: | :--- | :--- |
| **Q1** | ICMP Ping & ARP Resolution | Sent 4 pings to `1.1.1.1` (avg RTT $36.678\text{ ms}$, 0\% loss). Wireshark captures matching pairs (`seq=0..3`, `id=0x7838`). Initial ARP broadcast (`Who has 192.168.0.1?`) resolved gateway MAC `ba:5f:0b:e5:dc`. |
| **Q2a** | Unfiltered Protocols | ARP, UDP, IGMPv3, DNS, QUIC (HTTP/3), MDNS, TCP, HTTP, TLSv1.2/1.3, ICMP. |
| **Q2b** | HTTP GET to 200 OK Latency | $\Delta t = 338.985365 - 338.603803 = \mathbf{0.381562\text{ s}}$ ($\mathbf{381.562\text{ ms}}$). In Time-of-Day: $\Delta t = \mathbf{0.381682\text{ s}}$. Verified by Wireshark internal field `[Time since request: 0.381562 seconds]`. |
| **Q2c** | Internet IP Addresses | Website (`neverssl.com`): `34.223.124.45`. Local PC: `192.168.0.102`. |
| **Q2d & e** | HTTP GET & Host Value | Frame #63971 inspected in Packet Details panel. Value of `Host`: **`neverssl.com`**. |
| **Q3** | Hex & ASCII Highlighting | Selecting `Host: neverssl.com\r\n` highlights 20 bytes in Packet Bytes panel: `48 6f 73 74 3a 20 6e 65 76 65 72 73 73 6c 2e 63 6f 6d 0d 0a`. |
| **Q4** | First 4 Hex Bytes of Host | Parameter `"Host"` $\rightarrow$ `'H'` (`0x48`), `'o'` (`0x6f`), `'s'` (`0x73`), `'t'` (`0x74`). Answer: **`48 6f 73 74`**. |
| **Q5** | Follow TCP Stream | Stream 40 reconstructed bidirectional dialogue: Red text = Client HTTP GET (85 B); Blue text = Server HTTP 200 OK HTML payload (4,261 B). |
| **Q6** | Server HTTP Response Frame | Filter `tcp.srcport == 80`. Frame #63982 expanded Ethernet II layer reveals Dst MAC `ee:16:f1:8c:06:01` and Src MAC `98:ba:5f:0b:e5:dc`. |
| **Q7** | NIC Manufacturers | **PC's NIC:** Apple, Inc. (MacBook Air; macOS Private Wi-Fi randomized MAC `ee:16:f1:8c:06:01` sets Locally Administered LG bit = 1).<br>**Server's Incoming Frame NIC:** TP-Link Technologies (MAC `98:ba:5f:0b:e5:dc`). Remote server physical NIC is hidden across L3 boundaries; local gateway transmits the L2 frame. |
| **Q8** | Manufacturer OUIs (Hex) | **Source (TP-Link Router):** **`98:ba:5f`** (`0x98 0xba 0x5f`).<br>**Destination (MacBook Air):** **`ee:16:f1`** (`0xee 0x16 0xf1`, Locally Administered address). |
| **Q9** | Protocol Hierarchy Stats | Total packets: 97,552.<br>**TCP:** **74.8%** (72,994 pkts). Higher protocols: HTTP, HTTPS/TLS, SSH, FTP, XMPP.<br>**UDP:** **24.4%** IPv4 + 0.1% IPv6 = **24.57%** (23,970 pkts). Higher protocols: DNS, QUIC (HTTP/3), DHCP, NTP, mDNS. |
| **Q10** | Flow Graph Analysis | Chronological ladder diagram between `192.168.0.102`, `34.104.35.123`, `34.223.124.45`, and `23.11.38.161`. |

---

## Compiling the LaTeX Report
To compile `report.tex` into a PDF:
```bash
cd a5
make
# or directly:
pdflatex report.tex && pdflatex report.tex
```
Alternatively, upload `report.tex` together with the `images/` directory to [Overleaf](https://www.overleaf.com) to compile instantly.
