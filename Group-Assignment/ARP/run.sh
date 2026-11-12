#!/usr/bin/env bash
# ==============================================================================
# Address Resolution Protocol (ARP - RFC 826) Runner & Test Orchestrator
# CSE/PC/B/S/314 Computer Networks Laboratory - Jadavpur University
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure binaries are built
make all

echo ""
echo "=========================================================================="
echo "         RFC 826 Address Resolution Protocol (ARP) Suite                  "
echo "=========================================================================="
echo " 1) Run Automated Full Simulation (All 7 Scenarios)"
echo " 2) Run Unit Test Suite (Codec & Cache Aging)"
echo " 3) Start Standalone Virtual L2 Ethernet Switch (Port 8000)"
echo " 4) Launch Interactive Host CLI (e.g. HostA @ 192.168.1.10)"
echo " 5) Clean build artifacts"
echo "=========================================================================="
read -p "Select option [1-5] (default 1): " choice
choice=${choice:-1}

case "$choice" in
    1)
        echo -e "\n[*] Launching Automated Multi-Node ARP Simulation..."
        ./bin/arp_sim
        ;;
    2)
        echo -e "\n[*] Running Unit Test Suite..."
        make test
        ;;
    3)
        echo -e "\n[*] Starting Virtual L2 Ethernet Switch Daemon on UDP port 8000..."
        ./bin/switch_daemon 8000
        ;;
    4)
        echo -e "\n[*] Setting up interactive Host CLI..."
        read -p "Enter Host Name (e.g., HostA): " hname
        hname=${hname:-HostA}
        read -p "Enter IPv4 Address (e.g., 192.168.1.10): " hip
        hip=${hip:-192.168.1.10}
        read -p "Enter MAC Address (e.g., 00:11:22:33:44:0A): " hmac
        hmac=${hmac:-00:11:22:33:44:0A}
        read -p "Enter Virtual Switch UDP Port (default 8000): " sport
        sport=${sport:-8000}

        ./bin/host_cli "$hname" "$hip" "$hmac" "127.0.0.1" "$sport"
        ;;
    5)
        echo -e "\n[*] Cleaning build artifacts..."
        make clean
        echo "[+] Cleaned successfully."
        ;;
    *)
        echo "Invalid selection."
        exit 1
        ;;
esac
