#!/usr/bin/env bash
# ==============================================================================
# File Transfer Protocol (FTP - RFC 959) Runner & Test Orchestrator
# CSE/PC/B/S/314 Computer Networks Laboratory - Jadavpur University
# ==============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Ensure binaries are built
make all

echo ""
echo "=========================================================================="
echo "         RFC 959 File Transfer Protocol (FTP) Suite                       "
echo "         CSE/PC/B/S/314 Computer Networks Lab - Jadavpur University       "
echo "=========================================================================="
echo " 1) Run Automated Full Simulation (All 8 Scenarios)"
echo " 2) Run Unit Test Suite (Codec, Sandbox Virtual Jail, SHA-256)"
echo " 3) Start Standalone FTP Server Daemon (Default Port 2121)"
echo " 4) Launch Interactive FTP Client CLI (e.g. ftp> open localhost 2121)"
echo " 5) Clean build artifacts and test directories"
echo "=========================================================================="
read -p "Select option [1-5] (default 1): " choice
choice=${choice:-1}

case "$choice" in
    1)
        echo -e "\n[*] Launching Automated Multi-Scenario FTP Simulation..."
        ./bin/ftp_sim
        ;;
    2)
        echo -e "\n[*] Running Unit Test Suite..."
        make test
        ;;
    3)
        echo -e "\n[*] Starting FTP Server Daemon..."
        read -p "Enter Port to bind (default 2121): " sport
        sport=${sport:-2121}
        read -p "Enter Root Directory for Sandbox (default ./ftp_root): " sroot
        sroot=${sroot:-./ftp_root}
        mkdir -p "$sroot"
        ./bin/ftp_server -p "$sport" -d "$sroot"
        ;;
    4)
        echo -e "\n[*] Setting up interactive FTP Client..."
        read -p "Enter Server Host (default 127.0.0.1): " shost
        shost=${shost:-127.0.0.1}
        read -p "Enter Server Port (default 2121): " sport
        sport=${sport:-2121}
        ./bin/ftp_client "$shost" "$sport"
        ;;
    5)
        echo -e "\n[*] Cleaning build artifacts and temporary files..."
        make clean
        echo "[+] Cleaned successfully."
        ;;
    *)
        echo "Invalid selection."
        exit 1
        ;;
esac
