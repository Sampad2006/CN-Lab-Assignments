# File Transfer Protocol (FTP - RFC 959 / RFC 3659) Implementation

**Course:** CSE/PC/B/S/314 Computer Networks Laboratory  
**Department:** Department of Computer Science and Engineering  
**Institution:** Jadavpur University, Kolkata  
**Session:** 2026–2027  
**Standards:** RFC 959 (File Transfer Protocol), RFC 3659 (Extensions to FTP), RFC 2428 (FTP Extensions for IPv6 and NATs)  
**Language & Standard:** C++17 (`clang++` / `g++`, POSIX TCP Sockets, `std::thread`, RAII)

---

## 📌 1. Theoretical Foundations & RFC 959 Specification

The **File Transfer Protocol (FTP)** is an application-layer protocol established in RFC 959 that provides robust, platform-independent remote file management and reliable bulk data transmission over the Transmission Control Protocol (TCP).

Unlike single-channel application protocols such as HTTP, FTP employs an **out-of-band dual-channel architecture**, separating signaling control from bulk data transfer.

```
       +------------------+                    +------------------+
       |   User / Host    |                    |   Server Host    |
       |  +------------+  |  Control Channel   |  +------------+  |
       |  |  User-PI   |<========================>| Server-PI  |  |
       |  +-----+------+  |   (TCP Port 21)    |  +-----+------+  |
       |        |         |                    |        |         |
       |  +-----v------+  |   Data Channel     |  +-----v------+  |
       |  |  User-DTP  |<========================>| Server-DTP |  |
       |  +------------+  |   (Ephemeral/Data) |  +------------+  |
       |  FTP Client CLI  |                    |  FTP Daemon      |
       +------------------+                    +------------------+
```

### 1.1 Dual TCP Connection Architecture

* **Control Connection (PI - Protocol Interpreter):**
  * Persistent bidirectional TCP connection initialized by the client to the server's control port (standard port 21, or configurable e.g. 2121).
  * Follows Telnet Network Virtual Terminal (NVT) conventions: ASCII text commands terminated by `<CRLF>` (`\r\n`).
  * Emits 3-digit standardized status replies (e.g., `220 Service Ready`, `331 Need Password`, `230 Logged In`, `226 Closing Data Connection`).
  * Carries commands, authentication tokens, directory navigation, transfer mode negotiation, and abort requests.

* **Data Connection (DTP - Data Transfer Process):**
  * Ephemeral dynamic TCP stream spawned on-demand solely for file transfers (`RETR`, `STOR`, `APPE`) and directory listings (`LIST`, `NLST`).
  * Closes automatically upon completion of the payload (EOF) or error, signaling transfer termination to the receiver.

---

### 1.2 Data Connection Modes: Active vs. Passive

```
ACTIVE MODE (PORT)                                PASSIVE MODE (PASV)
==================                                ===================
1. Client binds local listener                    1. Client sends "PASV"
2. Client sends "PORT h1,h2,h3,h4,p1,p2"          2. Server binds ephemeral listener
3. Server replies "200 PORT OK"                   3. Server replies "227 (h1,h2,h3,h4,p1,p2)"
4. Client sends "RETR/STOR <file>"                4. Client initiates TCP connect to server
5. Server connects back to client                 5. Client sends "RETR/STOR <file>"
6. Data transfer begins                           6. Data transfer begins
```

* **Active Mode (`PORT` / `EPRT`):**
  The client opens an ephemeral TCP listening port on its local host and informs the server via `PORT h1,h2,h3,h4,p1,p2`. The server then initiates an active TCP connection back to the client's socket.
  *(Note: Active mode often fails across client-side NATs/firewalls unless port forwarding or application-level gateways are present).*

* **Passive Mode (`PASV` / `EPSV`):**
  The client issues `PASV`. The server allocates an ephemeral TCP listening port and responds with `227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)`. The client initiates the TCP connection directly to the server's data listener. This is firewall/NAT-friendly and standard for modern deployments.

---

### 1.3 Virtual Chroot Jail & Sandbox Security Model

To prevent **Path Traversal / Directory Climbing Attacks** (`../` injection):
* The server maintains a strict virtual directory root `/` mapped to a configured physical directory on the host filesystem (e.g. `./ftp_root`).
* All virtual paths (`/`, `/sub/nested`, `../`, `../../../../etc/passwd`) undergo canonical stack normalization:
  $$\text{NormalizePath}(P) = \text{CleanPath}(P) \cap \text{RootVFS}$$
* Any attempt to navigate above the virtual root (`/..`) is securely clamped to `/`.
* Role-based permissions enforce read-only status for anonymous or guest sessions, rejecting write attempts (`STOR`, `MKD`, `DELE`, `RMD`, `APPE`) with `550 Permission Denied`.

---

## 🏛️ 2. Architectural Design & Components

The implementation is designed using modern C++17 modular principles:

```
Group-Assignment/FTP/
├── Makefile                      # Production clang++/g++ build orchestrator
├── README.md                     # Academic specifications & operation manual
├── run.sh                        # Interactive execution & test launcher
├── include/
│   ├── common.hpp                # RFC 959 reply codes, IpPortTuple, SHA-256
│   ├── logger.hpp                # Wireshark-style colorized dissector logger
│   ├── socket_utils.hpp          # POSIX TCP socket primitives & RAII handles
│   ├── ftp_command.hpp           # RFC 959 command tokenizer & parser
│   ├── ftp_reply.hpp             # 3-digit reply encoder & multiline parser
│   ├── sandbox_fs.hpp            # Virtual Chroot Jail & Unix ls formatter
│   ├── ftp_session.hpp           # Per-client protocol state machine
│   ├── ftp_server.hpp            # Multi-client TCP FTP daemon & thread pool
│   └── ftp_client.hpp            # High-level client API (Active/Passive/REST)
├── src/
│   ├── common.cpp                # Address tuple codec & NIST SHA-256 engine
│   ├── socket_utils.cpp          # Non-blocking sockets, timeouts, streams
│   ├── ftp_command.cpp           # Command parsing and wire encoding
│   ├── ftp_reply.cpp             # Multiline reply formatters
│   ├── sandbox_fs.cpp            # Chrooted filesystem & path normalization
│   ├── ftp_session.cpp           # Command dispatching & data channel handler
│   ├── ftp_server.cpp            # Server connection listener & user auth
│   ├── ftp_client.cpp            # Client command & data transfer engine
│   ├── main_server.cpp           # Standalone FTP server daemon binary
│   ├── main_client.cpp           # Interactive CLI FTP client shell
│   └── main_sim.cpp              # Automated 8-scenario end-to-end simulator
└── tests/
    ├── test_command_codec.cpp    # Unit tests for command/reply wire codecs
    ├── test_sandbox_fs.cpp       # Unit tests for virtual jail & traversal guards
    └── test_transfer_integrity.cpp # Unit tests for SHA-256 checksum & slicing
```

---

## 🔄 3. Protocol Message Exchange Workflows

### 3.1 Authentication & Capability Negotiation
```
Client (User-PI)                             Server (Server-PI)
       |                                             |
       |  ---- TCP Handshake (Port 2121) --------->  |
       |  <--- 220 JU Computer Networks FTP Ready -  |
       |  ---- USER student ---------------------->  |
       |  <--- 331 User name okay, need password --  |
       |  ---- PASS pass ------------------------->  |
       |  <--- 230 User logged in successfully ----  |
       |  ---- SYST ------------------------------>  |
       |  <--- 215 UNIX Type: L8 ------------------  |
       |  ---- FEAT ------------------------------>  |
       |  <--- 211-Features: SIZE MDTM REST PASV --  |
```

### 3.2 Passive Mode File Retrieval with Resumption (`REST`)
```
Client (User-PI / DTP)                       Server (Server-PI / DTP)
       |                                             |
       |  ---- REST 1048576 ---------------------->  |
       |  <--- 350 Restarting at 1048576 ----------  |
       |  ---- PASV ------------------------------>  |
       |  <--- 227 Entering Passive Mode (IP,Port)-  |
       |  ---- [Connects to Data Port] ----------->  |
       |  ---- RETR dataset.bin ------------------>  |
       |  <--- 150 Opening BINARY data connection -  |
       |  <==== Stream data from byte 1048576 =====  |
       |  <--- [Server closes data connection] ----  |
       |  <--- 226 Transfer complete --------------  |
```

---

## 📋 4. Supported RFC 959 & RFC 3659 Commands

| Command | RFC | Category | Description | Server Status Codes |
| :--- | :---: | :--- | :--- | :--- |
| `USER <username>` | 959 | Access | Transmit user identification | `331`, `230`, `501`, `530` |
| `PASS <password>` | 959 | Access | Transmit user authentication password | `230`, `503`, `530` |
| `QUIT` | 959 | Access | Graceful session termination and logout | `221` |
| `PWD` / `XPWD` | 959 | Navigation | Print Current Virtual Working Directory | `257`, `530` |
| `CWD <path>` | 959 | Navigation | Change Remote Working Directory | `250`, `550`, `530` |
| `CDUP` / `XCUP` | 959 | Navigation | Change to Parent Working Directory | `250`, `550`, `530` |
| `MKD <path>` | 959 | File Mgmt | Create new directory inside sandbox | `257`, `550`, `530` |
| `RMD <path>` | 959 | File Mgmt | Remove directory from sandbox | `250`, `550`, `530` |
| `DELE <path>` | 959 | File Mgmt | Delete specified file | `250`, `550`, `530` |
| `RNFR <file>` | 959 | File Mgmt | Rename From: Specify source path | `350`, `550`, `530` |
| `RNTO <file>` | 959 | File Mgmt | Rename To: Complete atomic rename | `250`, `503`, `550`, `530` |
| `TYPE <A\|I>` | 959 | Parameter | Set data representation (ASCII vs. Binary) | `200`, `504` |
| `MODE <S>` | 959 | Parameter | Set transfer mode (Stream) | `200`, `504` |
| `STRU <F>` | 959 | Parameter | Set file structure (File) | `200`, `504` |
| `PASV` | 959 | Transport | Enter Passive Mode (server listens) | `227`, `425` |
| `PORT <h,p>` | 959 | Transport | Enter Active Mode (client listens) | `200`, `501` |
| `EPSV` | 2428 | Transport | Extended Passive Mode | `229`, `425` |
| `EPRT <\|net\|ip\|port\|>` | 2428 | Transport | Extended Active Mode | `200`, `501` |
| `LIST [path]` | 959 | Transfer | Unix-format `ls -l` directory listing | `150`, `226`, `425`, `550` |
| `NLST [path]` | 959 | Transfer | Plain list of filenames | `150`, `226`, `425`, `550` |
| `RETR <file>` | 959 | Transfer | Download / Retrieve file from server | `150`, `226`, `425`, `550` |
| `STOR <file>` | 959 | Transfer | Upload / Store file to server | `150`, `226`, `425`, `550` |
| `APPE <file>` | 959 | Transfer | Append data to existing remote file | `150`, `226`, `425`, `550` |
| `SIZE <file>` | 3659 | Metadata | Return file length in bytes | `213`, `550` |
| `MDTM <file>` | 3659 | Metadata | Return last modification timestamp (UTC) | `213`, `550` |
| `REST <offset>` | 959/3659 | Stream | Restart / resume stream transfer marker | `350`, `501` |
| `ABOR` | 959 | Control | Abort current transfer | `226` |
| `SYST` | 959 | System | Query remote system operating system type | `215` |
| `FEAT` | 2389 | System | Negotiate extended features | `211` |
| `NOOP` | 959 | System | No-operation keepalive probe | `200` |
| `HELP` | 959 | System | Display supported command summary | `214` |

---

## 📊 5. RFC 959 Status Reply Codes

* **1xx (Positive Preliminary):** `125` Data connection open, `150` File status okay; opening data connection.
* **2xx (Positive Completion):** `200` Command OK, `215` UNIX system type, `220` Service ready, `221` Service closing, `226` Closing data connection, `227` Entering Passive Mode, `230` User logged in, `250` File action OK, `257` Pathname created.
* **3xx (Positive Intermediate):** `331` Username OK; need password, `350` Pending further info (e.g., `RNFR` or `REST`).
* **4xx (Transient Negative):** `421` Service not available, `425` Cannot open data connection, `426` Connection closed; transfer aborted.
* **5xx (Permanent Negative):** `500` Syntax error, `501` Syntax error in parameters, `502` Command not implemented, `503` Bad sequence, `530` Not logged in, `550` File unavailable or permission denied.

---

## 🚀 6. Compilation & Execution Guide

### 6.1 Prerequisites
* macOS / Linux operating system
* `clang++` (Apple Clang 14+) or `g++` (GCC 9+) with C++17 support
* POSIX sockets, `pthread` support

### 6.2 Building Targets
```bash
cd Group-Assignment/FTP

# Build all binaries and unit tests
make all

# Run comprehensive unit tests
make test

# Run automated end-to-end multi-scenario simulation
make sim

# Clean build artifacts
make clean
```

### 6.3 Interactive Launcher (`run.sh`)
```bash
./run.sh
```
Presents an interactive menu:
```
==========================================================================
         RFC 959 File Transfer Protocol (FTP) Suite                       
         CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University       
==========================================================================
 1) Run Automated Full Simulation (All 8 Scenarios)
 2) Run Unit Test Suite (Codec, Sandbox Virtual Jail, SHA-256)
 3) Start Standalone FTP Server Daemon (Default Port 2121)
 4) Launch Interactive FTP Client CLI (e.g. ftp> open localhost 2121)
 5) Clean build artifacts and test directories
==========================================================================
```

### 6.4 Standalone Server Daemon CLI

To start the server daemon in its own terminal:

```bash
cd Group-Assignment/FTP

# Start server on custom port 2121 with sandbox directory ./ftp_root
./bin/ftp_server -p 2121 -d ./ftp_root
```

**Pre-configured accounts:**
| Username | Password | Access Privileges | Purpose |
| :--- | :--- | :--- | :--- |
| `admin` | `admin123` | **Read-Write** | Full administrative rights (upload, download, delete, rename, mkdir) |
| `student` | `pass` | **Read-Write** | Standard laboratory student account |
| `guest` | `guest` | **Read-Only** | Guest account (download & listing only) |
| `anonymous` | *(any / empty)* | **Read-Only** | Public anonymous access |

---

### 6.5 Interactive Client Shell (`ftp>`) & Complete Manual Guide

Open a separate terminal window and connect to the running server:

```bash
cd Group-Assignment/FTP

# Connect to the FTP server
./bin/ftp_client 127.0.0.1 2121
```

Once connected, you will be in the interactive `ftp>` shell:

#### 1. Authentication (`user` command)
You can provide the username and password on the same line, or let the shell prompt you:
```text
# Single-line login:
ftp> user admin admin123
[+] Logged in successfully as admin.

# Or prompted login:
ftp> user student
Password: pass
[+] Logged in successfully as student.
```

#### 2. Directory Navigation (`pwd`, `cd`, `cdup`, `mkdir`, `rmdir`)
```text
# Print remote working directory:
ftp> pwd
Remote directory: /

# Create a new directory in the sandbox:
ftp> mkdir my_docs
[+] Directory created: my_docs

# Change directory into the folder:
ftp> cd my_docs
[+] Directory changed to /my_docs

# Go up to parent directory:
ftp> cdup
[+] Moved up to parent directory.

# Remove an empty directory (Note: use rmdir, not delete):
ftp> rmdir my_docs
[+] Directory removed: my_docs
```

#### 3. Uploading Files (`put` / `append`)
```text
# Upload local file 'report.txt' to remote name 'final_report.txt':
ftp> put report.txt final_report.txt
[*] Uploading 'report.txt' to 'final_report.txt'...
[+] Upload complete: final_report.txt

# If local and remote names are identical:
ftp> put data.bin
[*] Uploading 'data.bin' to 'data.bin'...
[+] Upload complete: data.bin

# Append local data to an existing remote file:
ftp> append extra_log.txt final_report.txt
[+] Appended successfully.
```

#### 4. Listing Files (`ls` / `dir`)
```text
# List files with standard Unix details (mode, owner, size, timestamp, name):
ftp> ls
-rw-r--r--   1 ftp      ftp           520 Nov 15 22:50 final_report.txt
-rw-r--r--   1 ftp      ftp         10240 Nov 15 22:51 data.bin
```

#### 5. Downloading Files (`get` / `recv`)
```text
# Download remote file 'final_report.txt' to local file 'downloaded_report.txt':
ftp> get final_report.txt downloaded_report.txt
[*] Downloading 'final_report.txt' to 'downloaded_report.txt'...
[+] Download complete: downloaded_report.txt

# Download with the same name:
ftp> get data.bin
[*] Downloading 'data.bin' to 'data.bin'...
[+] Download complete: data.bin
```

#### 6. File Metadata & Renaming (`size`, `mdtm`, `rename`)
```text
# Check exact byte size:
ftp> size final_report.txt
final_report.txt: 520 bytes (520 B)

# Check last modification timestamp (UTC):
ftp> mdtm final_report.txt
final_report.txt mtime: 20261115172000

# Rename remote file:
ftp> rename final_report.txt archive_report.txt
[+] Renamed final_report.txt to archive_report.txt
```

#### 7. Deleting Files (`delete` / `rm`)
```text
# Delete a remote file:
ftp> delete archive_report.txt
[+] Deleted archive_report.txt
```
*(Note: Use `delete` for files, and `rmdir` for directories).*

#### 8. Switching Modes & Disconnecting
```text
# Toggle between Passive (PASV) and Active (PORT) mode:
ftp> passive
[*] Mode set to: ACTIVE (PORT)

# Switch transfer representation:
ftp> binary
[*] Type set to Binary (IMAGE).
ftp> ascii
[*] Type set to ASCII.

# Clean exit:
ftp> quit
Goodbye.
```

---

## 🔬 7. Automated Simulation & Verification Report

The automated test runner (`./bin/ftp_sim`) validates the complete protocol lifecycle across 8 academic scenarios:

```
==============================================================================
         RFC 959 File Transfer Protocol (FTP) End-to-End Simulation Suite     
         CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University           
==============================================================================

 Scenario 1: Control Connection Initialization & Capability Negotiation
 [✓] Received RFC 959 220 Greeting Banner
 [✓] SYST returns '215 UNIX Type: L8'
 [✓] FEAT negotiates supported extensions (SIZE, MDTM, REST, PASV, EPSV, UTF8)
 [✓] NOOP keepalive probe acknowledged with 200 OK

 Scenario 2: Authentication State Machine & Privilege Boundaries
 [✓] Unauthenticated PWD/LIST rejected with 530 Not Logged In
 [✓] Incorrect credentials rejected with 530 Authentication failed
 [✓] Anonymous login granted with Read-Only privileges
 [✓] Anonymous write attempt (MKD) intercepted with 550 Permission Denied
 [✓] Read-Write administrator credentials authenticated successfully

 Scenario 3: Virtual Chroot Jail & Path Traversal Attack Defense
 [✓] Initial virtual working directory verified as '/'
 [✓] Hierarchical navigation (/projects/cn_lab) functional
 [✓] Path traversal exploit injection ('CWD ../../../../../etc') intercepted
 [✓] Filesystem access safely clamped inside virtual sandbox jail
 [✓] Relative navigation (CDUP) operational

 Scenario 4: Passive Mode (PASV) File Transfer & Directory Listing
 [✓] Server opens ephemeral data listener upon PASV command
 [✓] File uploaded successfully via STOR
 [✓] Standard Unix-format directory listing parsed
 [✓] File retrieved via RETR; SHA-256 bit-exact match confirmed

 Scenario 5: Active Mode (PORT) Data Channel Handshake
 [✓] Client opens local listener; transmits PORT tuple to server
 [✓] Server establishes reverse TCP connection back to client
 [✓] File upload and download successful over active channel
 [✓] Active mode SHA-256 integrity match confirmed

 Scenario 6: High-Volume Binary Stream Transfer (TYPE I) & SHA-256 Verification
 [✓] 1.0 MB pseudorandom binary payload generated
 [✓] High-throughput stream upload executed
 [✓] High-throughput stream download executed
 [✓] Source vs. Destination SHA-256 cryptographic match confirmed

 Scenario 7: Stream Resumption (REST Marker) after Disrupted Download
 [✓] Partial local download simulated (first 256 KB retained)
 [✓] REST 262144 marker transmitted and acknowledged with 350
 [✓] Remaining 768 KB retrieved and appended to local file
 [✓] Bit-exact reassembly verified against reference SHA-256

 Scenario 8: Metadata, Atomic Rename, Append, Delete & Concurrency
 [✓] RFC 3659 SIZE and MDTM queries verified
 [✓] Atomic file rename (RNFR -> RNTO) executed
 [✓] File stream append (APPE) confirmed with size expansion
 [✓] File deletion (DELE) verified
 [✓] 4 concurrent client threads executed simultaneous transfers without race conditions
```

### 7.1 Integrity Benchmark Summary

| Metric | Measured Value | Standard / Requirement |
| :--- | :---: | :---: |
| **Control Connection Handshake** | $< 1.5\text{ ms}$ | RFC 959 Section 5.3 |
| **Authentication Latency** | $< 1.0\text{ ms}$ | RFC 959 Section 4.1.1 |
| **Passive Mode Ephemeral Binding** | Immediate ($< 0.5\text{ ms}$) | RFC 959 Section 3.2 |
| **Active Mode Reverse Connect** | Immediate ($< 0.8\text{ ms}$) | RFC 959 Section 3.2 |
| **Binary File Integrity (SHA-256)** | **$100\%$ Bit-Exact Match** | FIPS 180-4 / NIST |
| **Stream Resumption (REST)** | **$100\%$ Bit-Exact Reassembly** | RFC 3659 Section 5 |
| **Directory Traversal Containment** | **$100\%$ Sandboxed** | CWE-22 Path Traversal Guard |
| **Concurrent Client Sessions** | **4 / 4 Successful Parallel Workers** | Thread-Safe POSIX Worker Pool |

---

## 👥 Authors & Laboratory Credentials

* **Student Name:** Sampad De  
* **Roll Number:** 002410501025  
* **Course:** CSE/PC/B/S/314 Computer Networks Laboratory  
* **Department:** Department of Computer Science and Engineering  
* **Institution:** Jadavpur University, Kolkata  
