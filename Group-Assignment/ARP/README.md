# Address Resolution Protocol (ARP - RFC 826) Implementation

**Course:** CSE/PC/B/S/314 Computer Networks Laboratory  
**Department:** Department of Computer Science and Engineering  
**Institution:** Jadavpur University, Kolkata  
**Session:** 2026–2027  
**Standard:** RFC 826 (Ethernet Address Resolution Protocol)  
**Language & Standard:** C++17 (`clang++` / `g++`, POSIX sockets, `pthread`)

---

## 📌 1. Theoretical Foundations & RFC 826 Specification

The **Address Resolution Protocol (ARP)** operates between the Data Link Layer (Layer 2) and Network Layer (Layer 3) of the OSI model. Its primary responsibility is dynamically discovering the hardware mapping (48-bit IEEE 802 MAC address) corresponding to a known internet protocol address (32-bit IPv4 address) within the same local broadcast domain.

### 1.1 RFC 826 Packet Structure

The binary packet format adheres byte-for-byte to RFC 826 using `#pragma pack(push, 1)`:

```
 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|       Hardware Type (HTYPE)   |       Protocol Type (PTYPE)   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| HLEN (6 bytes)| PLEN (4 bytes)|          Opcode (OPER)        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Sender Hardware Address (SHA: bytes 0-3)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  SHA (bytes 4-5)              |  Sender Protocol Address (SPA)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|  SPA (bytes 2-3)              |  Target Hardware Address (THA)|
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Target Hardware Address (THA: bytes 2-5)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                  Target Protocol Address (TPA: bytes 0-3)     |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

| Field | Size | Default Value | Description |
| :--- | :---: | :---: | :--- |
| **HTYPE** | 16 bits | `0x0001` | Hardware Type (Ethernet). |
| **PTYPE** | 16 bits | `0x0800` | Protocol Type (IPv4). |
| **HLEN**  | 8 bits  | `6`      | Hardware Address Length (6 octets for IEEE 802 MAC). |
| **PLEN**  | 8 bits  | `4`      | Protocol Address Length (4 octets for IPv4). |
| **OPER**  | 16 bits | `1` or `2` | Opcode: `1` = ARP Request, `2` = ARP Reply. |
| **SHA**   | 48 bits | Variable | Sender Hardware (MAC) Address. |
| **SPA**   | 32 bits | Variable | Sender Protocol (IPv4) Address. |
| **THA**   | 48 bits | Variable | Target Hardware (MAC) Address (`00:00:00:00:00:00` in requests). |
| **TPA**   | 32 bits | Variable | Target Protocol (IPv4) Address. |

### 1.2 Ethernet II Frame Encapsulation

Every ARP payload (28 bytes) is encapsulated inside a standard Ethernet II frame (14-byte header):

```
+--------------------------------------------------------------------------+
| Dest MAC (6B) | Src MAC (6B) | EtherType (2B: 0x0806) | ARP Payload (28B)|
+--------------------------------------------------------------------------+
```

* **Broadcast ARP Request**: `Dest MAC = FF:FF:FF:FF:FF:FF`, flooded across the local broadcast domain.
* **Unicast ARP Reply**: `Dest MAC = Requesting Host's MAC`, routed directly by the L2 switch.
* **Post-Resolution Data Frame**: `EtherType = 0x88B5` (or `0x0800`), containing higher-layer application payload transmitted directly to resolved MAC.

---

## 🏛️ 2. Architectural Design & Components

The implementation is structured into modular components:

```
Group-Assignment/ARP/
├── Makefile                      # Standard build rules (compile, test, clean)
├── README.md                     # Academic specifications & operation manual
├── run.sh                        # Interactive execution & demo launcher
├── include/
│   ├── types.hpp                 # MacAddress & IpAddress primitives, hashers
│   ├── ethernet.hpp              # Ethernet II frame header and binary codec
│   ├── arp_packet.hpp            # RFC 826 packet structures & serializers
│   ├── arp_cache.hpp             # Thread-safe ARP Cache table & TTL aging
│   ├── host_node.hpp             # Virtual station, socket engine & packet queue
│   ├── virtual_switch.hpp        # Virtual L2 Ethernet Switch & CAM table
│   └── logger.hpp                # Colorized Wireshark-like dissector logger
├── src/
│   ├── types.cpp                 # String parsing & formatting
│   ├── arp_packet.cpp            # RFC 826 packet packing, unpacking & dissector
│   ├── arp_cache.cpp             # Cache insertion, state machine, eviction
│   ├── virtual_switch.cpp        # L2 CAM MAC learning, flooding & unicast forwarding
│   ├── host_node.cpp             # ARP Request/Reply state machine, pending queue
│   ├── main_sim.cpp              # Automated 7-scenario simulator
│   ├── main_host.cpp             # Standalone interactive Host CLI
│   └── main_switch.cpp           # Standalone Virtual Switch daemon
└── tests/
    ├── test_codec.cpp            # Binary codec & network byte order tests
    └── test_arp_cache.cpp        # Cache state transitions, TTL aging, static protection
```

### 2.1 Core Architectural Features

1. **Virtual L2 Ethernet Switch (`VirtualSwitch`)**:
   * Simulates an IEEE 802.1D bridge communicating with host nodes over UDP.
   * **CAM Learning Table**: Automatically registers the source MAC and UDP endpoint of arriving frames.
   * **Intelligent Forwarding**: Floods broadcast frames (`FF:FF:FF:FF:FF:FF`) across all connected ports except the sender, while unicasting frames directly to the learned port.

2. **Outbound Packet Buffering (`pending_queue_`)**:
   * When an application sends higher-layer data to a destination IP whose MAC is currently unresolved (Cache MISS), the host buffers the packet in an internal queue.
   * Host sends an ARP Request broadcast. Upon arrival of the ARP Reply, the MAC mapping is saved and all queued packets are automatically released and transmitted.

3. **Provision for Displaying IP-to-MAC Mappings at Each Host (`show arp` / `arp -a`)**:
   * Displays the host's active ARP table with columns: IP Address, MAC Address, State (`REACHABLE`, `STATIC`, `INCOMPLETE`, `EXPIRED`), remaining TTL, and HW Type.

4. **Dynamic TTL Aging & Cache Invalidation**:
   * Cache entries transition through RFC lifecycle states. A background timer thread audits entries every second and garbage-collects expired records.

5. **Security & ARP Spoofing Mitigation**:
   * Supports `STATIC` permanent entries that are immune to dynamic overwrites, providing defense against Man-in-the-Middle (MitM) ARP poisoning attacks.
   * Gratuitous ARP (GARP) support for interface IP announcement and duplicate IP conflict detection.

---

## 🚀 3. Compilation & Quick Start

### 3.1 Build Targets

```bash
cd Group-Assignment/ARP

# Build all binaries
make all

# Run automated unit test suite
make test

# Launch automated multi-scenario simulation
make sim
```

### 3.2 Running the Interactive Launcher

```bash
./run.sh
```

---

## 🔬 4. Experimental Demonstrations in `arp_sim`

The automated simulator (`bin/arp_sim`) demonstrates seven scenarios:

```
+-------------------------------------------------------------------------------+
| Scenario 1: Initial Cold ARP Tables                                           |
|   - Verifies empty initial state across Host A and Host B.                    |
+-------------------------------------------------------------------------------+
| Scenario 2: Cold Resolution & UDP Transmission                                |
|   - Host A sends data to Host B -> Cache MISS -> Outbound data buffered.      |
|   - Host A broadcasts ARP Request -> Switch floods frame.                     |
|   - Host B updates cache with Host A's mapping and unicasts ARP Reply.        |
|   - Host A receives reply, updates cache, and flushes queued data frame.      |
+-------------------------------------------------------------------------------+
| Scenario 3: Warm Cache Hit                                                    |
|   - Subsequent data transmissions use fast-path without ARP overhead.         |
+-------------------------------------------------------------------------------+
| Scenario 4: Virtual Switch CAM Table Inspection                               |
|   - Displays learned MAC -> UDP Port mappings in switch forwarding table.     |
+-------------------------------------------------------------------------------+
| Scenario 5: Cache TTL Aging & Automatic Re-Resolution                         |
|   - Entries age out after TTL countdown -> subsequent transmission triggers   |
|     fresh ARP Request/Reply.                                                  |
+-------------------------------------------------------------------------------+
| Scenario 6: Gratuitous ARP & Duplicate IP Conflict Detection                  |
|   - Rogue host boots with an already claimed IP; Host A flags conflict.      |
+-------------------------------------------------------------------------------+
| Scenario 7: ARP Spoofing / Poisoning & Static Defense                         |
|   - Attacker attempts cache poisoning via unsolicited ARP reply.              |
|   - Demonstrates vulnerability of dynamic cache and mitigation via STATIC.    |
+-------------------------------------------------------------------------------+
```

---

## 💻 5. Multi-Process Interactive Host CLI

For manual, multi-terminal testing:

### Terminal 1: Virtual Switch
```bash
./bin/switch_daemon 8000
```

### Terminal 2: Host A
```bash
./bin/host_cli HostA 192.168.1.10 00:11:22:33:44:0A 127.0.0.1 8000
```

### Terminal 3: Host B
```bash
./bin/host_cli HostB 192.168.1.20 00:11:22:33:44:0B 127.0.0.1 8000
```

### Available Host Commands:
* `show arp` / `arp -a`: Inspect local IP-to-MAC resolution table.
* `send <target_ip> <message>`: Send data frame (triggers resolution if needed).
* `ping <target_ip>`: Send test ping frame.
* `gratuitous`: Broadcast Gratuitous ARP announcement.
* `static <ip> <mac>`: Add protected static mapping.
* `poison <v_ip> <v_mac> <fake_ip> <fake_mac>`: Send malicious spoofed reply.
* `flush`: Clear dynamic ARP cache.
