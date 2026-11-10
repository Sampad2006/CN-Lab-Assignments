# Assignment 3: Comprehensive Evaluation Report — Medium Access Control (CSMA & CSMA/CD)

**Course:** CSE/PC/B/S/314 Computer Networks Lab (CO3)  
**Topic:** Design and Implementation of Medium Access Control Mechanisms within a Simulated Network Environment using IEEE 802 Standards  
**Student:** Sampad De (Roll No: 002410501025)  
**Language & Platform:** C++17, POSIX Datagram IPC Sockets, Python 3, macOS  

---

## 1. Title & Executive Summary

This report documents the design, implementation, and empirical evaluation of Carrier Sense Multiple Access (CSMA) medium access control protocols implemented in directory `a3/`:
1. **Non-Persistent CSMA**
2. **1-Persistent CSMA**
3. **p-Persistent CSMA** (with varying transmission probability $p$)
4. **CSMA with Collision Detection (CSMA/CD)** (IEEE 802.3 Ethernet MAC with Jamming and Binary Exponential Backoff [BEB])

### Core Performance Metrics Defined:
* **Total Collisions ($C_{\text{total}}$):** Number of overlapping transmission events where two or more stations attempt simultaneous transmission over the shared channel.
* **Average Transmission Delay ($\bar{D}_{\text{tx}}$):** Total wall-clock duration elapsed from when a frame is generated / enqueued at a station until it is successfully transmitted and delivered to the channel receiver (including sensing wait times, deferrals, backoffs, and collision recovery times):
  $$\bar{D}_{\text{tx}} = \frac{1}{M} \sum_{i=1}^{M} (T_{\text{delivered}, i} - T_{\text{ready}, i})$$
* **Effective Channel Throughput ($S_{\text{eff}}$):** Useful payload data successfully delivered over the total simulation / elapsed channel time:
  $$S_{\text{eff}} = \frac{\text{Total Successful Payload Bits Delivered}}{\text{Total Elapsed Time}} = \frac{M_{\text{success}} \times 8 \times L_{\text{payload}}}{T_{\text{elapsed}}} \quad (\text{kbps})$$
* **Channel Utilization ($\eta_{\text{channel}}$):** The percentage of channel wall-clock time occupied by successful, collision-free frame transmissions:
  $$\eta_{\text{channel}} = \frac{\sum T_{\text{successful\_tx}}}{T_{\text{elapsed}}} \times 100\%$$

---

## 2. Design & Architecture

### 2.1 System Procedural Organization
The system follows a distributed client-server architecture emulating a shared physical broadcast medium (Ethernet bus / wireless channel):

```
       +--------------------------------------------------------+
       |               Shared Channel Emulator                  |
       |                (server / channel.cpp)                  |
       |  State Machine: [ IDLE | BUSY | COLLISION ]            |
       |  - Tracks active transmitters                          |
       |  - Senses carrier state                                |
       |  - Arbitrates simultaneous transmissions               |
       |  - Emulates propagation delay & collision window       |
       +--------------------------------------------------------+
               ^                    ^                    ^
               | (SENSE/TX/JAM)     | (SENSE/TX/JAM)     | (SENSE/TX/JAM)
               v                    v                    v
        +--------------+     +--------------+     +--------------+
        |  Station 1   |     |  Station 2   |     |  Station N   |
        |  (station.cpp|     |  (station.cpp|     |  (station.cpp|
        |   csma.cpp)  |     |   csma.cpp)  |     |   csma.cpp)  |
        |  Queue / BEB |     |  Queue / BEB |     |  Queue / BEB |
        +--------------+     +--------------+     +--------------+
```

### 2.2 Protocol State Machines & Logic

```
   [ Non-Persistent CSMA ]             [ 1-Persistent CSMA ]
          |                                      |
     Sense Channel                          Sense Channel
     /          \                           /          \
  (IDLE)      (BUSY)                     (IDLE)      (BUSY)
   /              \                       /              \
Transmit     Wait Random Backoff      Transmit       Wait & Poll Continuously
Packet       & Sense Again            Packet         until Channel becomes IDLE


   [ p-Persistent CSMA ]               [ CSMA / CD (IEEE 802.3) ]
          |                                      |
     Sense Channel                          Sense Channel (1-Persistent)
     /          \                                |
  (IDLE)      (BUSY)                        Wait until IDLE
   /              \                              |
Roll u in [0,1]   Wait until IDLE           Start Transmission (T_tx)
  /          \                                   |
(u <= p)   (u > p)                          Listen to wire during transmission?
 /              \                           /                         \
Transmit    Wait 1 Slot (tau)          (Collision Detected!)      (No Collision)
Packet      & Sense in Next Slot          /                           \
                                   Abort Transmission Immediately     Complete Tx
                                   Emit 32-bit Jam Signal             Success!
                                   Increment Retry k <- k+1
                                   Execute Binary Exponential Backoff
                                   Wait r * tau & Restart Sensing
```

### 2.3 Input and Output Format
* **Input Parameters:**
  * Protocol selector (`-p non_persistent | 1_persistent | p_persistent | csma_cd`)
  * Number of contending stations ($N \in [1, 30]$)
  * Transmission probability parameter for $p$-persistent ($p \in [0.01, 1.0]$)
  * Number of frames per station ($M = 10 - 20$)
  * Frame payload size ($L = 64\text{ bytes}$)
  * Slot duration ($\tau = 1.0\text{ ms}$) and Frame Transmission time ($T_{\text{tx}} = 5.0\text{ ms}$)
* **Output Format:**
  * Real-time formatted console telemetry for sensing queries, transmission starts, collision alerts, jam emissions, and backoff timers.
  * Summary tables listing attempted frames, success rates, collision counts, jam signals, mean delay per frame, throughput in kbps, and medium efficiency.
  * Structured CSV datasets and vector SVG plots.

---

## 3. Implementation Details

### 3.1 Packet Header Structure & Protocol Enums (`include/frame.hpp`)
```cpp
enum class ChannelState : uint8_t { IDLE = 0, BUSY = 1, COLLISION = 2 };
enum class ProtocolType : uint8_t { NON_PERSISTENT = 0, ONE_PERSISTENT = 1, P_PERSISTENT = 2, CSMA_CD = 3 };
enum class MsgType : uint8_t {
    MSG_SENSE_REQ = 1, MSG_SENSE_RESP = 2, MSG_START_TX = 3,
    MSG_CHECK_CD = 4, MSG_END_TX = 5, MSG_JAM = 6,
    MSG_STATS_REQ = 7, MSG_STATS_RESP = 8, MSG_RESET_REQ = 9
};

#pragma pack(push, 1)
struct MessageHeader {
    uint8_t msg_type;
    uint32_t station_id;
    uint32_t frame_seq;
    uint8_t channel_state;
    uint32_t duration_ms;
    uint8_t collided;
    uint32_t extra_info;
    uint64_t timestamp_us;
    uint32_t payload_len;
};
#pragma pack(pop)
```

### 3.2 Channel State Machine & Arbitration (`src/channel.cpp`)
The channel server handles concurrency through mutex-locked state transitions:
* When a station calls `handle_start_tx()`:
  * If `active_transmissions_` is empty: State $\rightarrow$ `BUSY`.
  * If `active_transmissions_` is non-empty: State $\rightarrow$ `COLLISION`. All overlapping active senders are flagged as `collided = true`, and a collision event is registered.
* When a station calls `handle_check_cd(station_id)`: returns whether the current transmission was corrupted.
* When a station calls `handle_jam(station_id)`: sets state to `COLLISION` and records jam sequence.
* When all active transmissions complete (`handle_end_tx()`): State returns to `IDLE`.

### 3.3 Truncated Binary Exponential Backoff (BEB) Implementation (`src/csma.cpp`)
```cpp
double CsmaStation::compute_beb_backoff(int retry_k) {
    int max_k = std::min(retry_k, config_.backoff_limit); // Truncated at k=10
    int max_slots = (1 << max_k) - 1;                     // Range [0, 2^k - 1]
    if (max_slots < 1) max_slots = 1;
    std::uniform_int_distribution<int> dist(0, max_slots);
    int r = dist(rng_);
    return r * config_.slot_time_ms;
}
```

---

## 4. Test Cases & Verification Setup

| Test Case | Description | Tested Parameters | Success Criteria |
| :---: | :--- | :--- | :--- |
| **TC-1** | Frame Serialization & Protocol Header Integrity | Frame size: 64B, OpCodes 1-10 | Byte-exact serialization & deserialization without data corruption |
| **TC-2** | Shared Medium State Transitions | IDLE $\rightarrow$ BUSY $\rightarrow$ COLLISION $\rightarrow$ IDLE | State changes match concurrent station count accurately |
| **TC-3** | Collision Detection & Jam Sequence | Simultaneous start at $t=0$ | Channel flags COLLISION; stations abort and broadcast Jam signal |
| **TC-4** | $p$-Persistent Parameter Sweep ($p \in [0.01, 1.0]$) | Fixed $N=5$, $M=20$ frames/station | Identifies optimal $p^*$, analyzes collision growth with $p$ |
| **TC-5** | Comprehensive Comparison of All 4 Schemes vs $N$ | $N \in [1, 30]$ contending stations | Evaluates stability, collision counts, delay, and throughput across loads |

---

## 5. Experimental Results

### 5.1 Experiment (i): $p$-Persistent CSMA vs. Transmission Probability $p$ (Fixed $N=5$)

*Setup:* $N = 5$ contending stations, $M = 10$ frames/station ($50$ total frames), Frame payload $64\text{ bytes}$, $T_{\text{tx}} = 5.0\text{ ms}$, $\tau = 1.0\text{ ms}$, averaged over $3$ independent trials per point.

| Probability $p$ | Contending Stations $N$ | Collisions | Average Delay (ms) | Throughput (kbps) | Tx Efficiency $\eta_{\text{tx}}$ (%) | Air Utilization $\eta_{\text{channel}}$ (%) |
| :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **0.01** | 5 | `0.7` | `141.41` | `13.74` | `98.72%` | `13.42%` |
| **0.05** | 5 | `10.0` | `53.68` | `38.12` | `83.40%` | `37.22%` |
| **0.10** | 5 | `17.0` | `43.85` | `47.02` | `74.73%` | `45.91%` |
| **0.15** | 5 | `37.0` | `45.92` | `45.23` | `57.91%` | `44.16%` |
| **0.20** | 5 | `42.0` | `45.63` | `42.01` | `55.96%` | `41.02%` |
| **0.30** | 5 | `76.7` | `55.65` | `33.41` | `39.73%` | `32.62%` |
| **0.40** | 5 | `89.3` | `60.38` | `29.45` | `36.25%` | `28.75%` |
| **0.50** | 5 | `85.0` | `60.57` | `27.03` | `37.29%` | `26.39%` |
| **0.60** | 5 | `76.7` | `54.94` | `33.46` | `39.72%` | `32.67%` |
| **0.70** | 5 | `78.3` | `55.16` | `30.89` | `39.05%` | `30.16%` |
| **0.80** | 5 | `65.0` | `48.82` | `31.13` | `43.94%` | `30.40%` |
| **0.90** | 5 | `62.3` | `40.74` | `44.14` | `46.02%` | `43.10%` |
| **1.00** | 5 | `59.0` | `49.06` | `31.16` | `46.10%` | `30.43%` |

---

### 5.2 Experiment (ii): Comparative Analysis of All Four Schemes vs. Contending Stations $N$

*Setup:* $N \in [1, 30]$ stations, $M = 10$ frames/station, $T_{\text{tx}} = 5.0\text{ ms}$, $\tau = 1.0\text{ ms}$, averaged over $3$ independent trials per configuration.

#### Comparison Table:

| Scheme | $N=1$ | $N=2$ | $N=4$ | $N=6$ | $N=8$ | $N=10$ | $N=15$ | $N=20$ | $N=25$ | $N=30$ |
| :--- | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
| **Non-Persistent Collisions** | `0.0` | `8.7` | `41.0` | `71.7` | `138.3`| `156.7`| `296.0`| `437.0`| `593.7`| `780.0`|
| **1-Persistent Collisions** | `0.0` | `3.3` | `31.7` | `66.0` | `96.7` | `115.7`| `191.3`| `291.7`| `412.3`| `540.3`|
| **p-Persistent ($p=0.1$) Collisions**| `0.0` | `2.7` | `16.0` | `37.7` | `68.0` | `125.7`| `251.0`| `376.0`| `594.7`| `807.7`|
| **CSMA/CD Collisions** | `0.0` | `4.0` | `37.7` | `62.0` | `86.3` | `110.0`| `192.3`| `276.7`| `386.3`| `446.3`|
| **Non-Persistent Delay (ms)** | `6.07` | `13.89`| `33.57`| `56.53`| `84.92`| `93.98`| `162.18`| `209.15`| `251.29`| `288.75`|
| **1-Persistent Delay (ms)** | `6.27` | `11.70`| `26.55`| `48.92`| `62.42`| `68.56`| `100.93`| `131.62`| `168.04`| `212.44`|
| **p-Persistent ($p=0.1$) Delay (ms)**| `14.92`| `25.08`| `37.41`| `57.10`| `76.61`| `98.89`| `157.47`| `206.42`| `277.30`| `341.85`|
| **CSMA/CD Delay (ms)** | `6.46` | `11.90`| `23.72`| `37.43`| `48.09`| `58.45`| `82.59`| `110.83`| `135.20`| `149.57`|
| **Non-Persistent Throughput (kbps)** | `69.85`| `53.66`| `43.93`| `31.92`| `32.64`| `31.45`| `27.84`| `30.24`| `31.79`| `31.77`|
| **1-Persistent Throughput (kbps)** | `67.98`| `77.18`| `51.33`| `29.98`| `34.62`| `36.46`| `42.72`| `43.59`| `40.89`| `36.86`|
| **p-Persistent ($p=0.1$) Throughput** | `32.24`| `35.51`| `45.81`| `43.54`| `41.66`| `39.61`| `35.57`| `33.34`| `34.65`| `31.98`|
| **CSMA/CD Throughput (kbps)** | `66.28`| `76.15`| `53.71`| `41.36`| `40.72`| `35.90`| `44.09`| `48.82`| `44.78`| `49.55`|
| **CSMA/CD Protocol Efficiency (%)** | `100.0%`| `83.7%`| `52.0%`| `49.8%`| `48.1%`| `47.7%`| `43.8%`| `42.1%`| `39.3%`| `40.4%`|
| **CSMA/CD Air Utilization (%)** | `64.7%`| `74.4%`| `52.4%`| `40.4%`| `39.8%`| `35.1%`| `43.1%`| `47.7%`| `43.7%`| `48.4%`|

---

## 6. Theoretical & Comparative Analysis

### 6.1 Dual Efficiency Metrics: $\eta_{\text{tx}}$ vs. $\eta_{\text{channel}}$
1. **Protocol Transmission Efficiency ($\eta_{\text{tx}}$):**
   $$\eta_{\text{tx}} = \frac{\text{Successful Frames}}{\text{Total Transmission Attempts}} \times 100\% = \frac{S}{S + C} \times 100\%$$
   Measures contention arbitration quality. When $p$ is small ($p=0.01$), $\eta_{\text{tx}} = 98.7\%$ as nearly zero collisions occur. As contention increases, $\eta_{\text{tx}}$ drops as multiple stations collide.
2. **Channel Air Utilization ($\eta_{\text{channel}}$):**
   $$\eta_{\text{channel}} = \frac{\text{Total Time Medium Spent on Useful Payload}}{\text{Total Simulation Duration}} \times 100\%$$
   Measures physical channel occupancy. In CSMA/CD, colliding frames are aborted within $\sim 2\tau + T_{\text{jam}} \approx 3.0\text{ ms}$, preserving high air utilization ($\approx 48.4\%$) even under 30 contending stations.

### 6.2 Collision Behavior & Channel Vulnerability
In pure CSMA without collision detection, when stations transmit during the vulnerable propagation window ($\tau$), the entire duration of both frames ($T_{\text{tx}} = 5\text{ ms}$) is wasted on the channel.
In **CSMA/CD**, when a collision is detected via *Listen-While-Talk*, transmission is aborted immediately and a Jam signal is broadcast for $T_{\text{jam}}$. The wasted channel time per collision is bounded by:
$$T_{\text{wasted, CSMA/CD}} = 2\tau + T_{\text{jam}} \ll T_{\text{tx}}$$
For our experimental configuration ($T_{\text{tx}} = 5.0\text{ ms}$, $\tau = 1.0\text{ ms}$, $T_{\text{jam}} = 1.0\text{ ms}$):
$$T_{\text{wasted, CSMA/CD}} = 2(1.0) + 1.0 = 3.0\text{ ms} < 5.0\text{ ms} \quad (40\%\text{ airtime savings per collision})$$

### 6.3 Optimal Probability $p^*$ Derivation
For $N$ contending stations, the probability that exactly one station successfully transmits in an idle slot is given by the binomial distribution:
$$P(\text{Success}) = N p (1 - p)^{N - 1}$$
Maximizing with respect to $p$:
$$\frac{d}{dp} [N p (1 - p)^{N - 1}] = 0 \implies p^* = \frac{1}{N}$$
For $N=5$, theoretical $p^* = \frac{1}{5} = \mathbf{0.20}$. In our empirical sweep (Table 5.1), $p \in [0.10, 0.20]$ represents the optimal inflection point achieving maximum throughput ($47.02\text{ kbps}$) and minimum delay ($43.85\text{ ms}$).

---

## 7. Observations & Key Takeaways

1. **Impact of Contending Stations ($N$):** Under heavy contention ($N=30$), CSMA/CD demonstrates superior resilience, achieving the lowest average delay ($149.57\text{ ms}$) and highest throughput ($49.55\text{ kbps}$) compared to all other schemes.
2. **Binary Exponential Backoff (BEB):** BEB dynamically adapts the contention window size ($2^{\min(k, 10)}$) to the level of channel congestion. When collisions occur, doubling the backoff window disperses retransmission attempts, preventing persistent deadlock.
3. **Slotted $p$-Persistent Mechanics:** Fixed $p=0.1$ is ideal for $N \le 10$. For higher $N$ ($N=30$), $N \cdot p = 3.0 > 1$, causing collisions to rise, proving that $p$ must scale inversely with active station count ($p \sim 1/N$).
4. **Lab Verification:** The concurrent multi-threaded simulator and standalone UNIX/UDP socket executables validate the foundational data link layer MAC algorithms defined in IEEE 802.3 and network literature.
