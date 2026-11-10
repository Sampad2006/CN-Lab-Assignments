#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${CYAN}================================================================${NC}"
echo -e "${BOLD}${CYAN}  CN Lab Assignment 4: CDMA Multiple Access with Walsh Codes    ${NC}"
echo -e "${BOLD}${CYAN}================================================================${NC}"
echo -e "1) Build All Executables (make all)"
echo -e "2) Run Unit Test Suite (make test)"
echo -e "3) Launch Shared Common Channel (Server, default N=4)"
echo -e "4) Launch Interactive Station Client"
echo -e "5) Run Multi-Station Headless Simulation Demo (N=4, 30 rounds)"
echo -e "6) Run Simultaneous Multi-Station Text Broadcast Demo"
echo -e "7) Run Bit Error Rate (BER) vs Channel Noise Benchmark"
echo -e "8) Clean Build Artifacts (make clean)"
echo -e "----------------------------------------------------------------"
read -p "Select an option [1-8]: " opt

case "$opt" in
    1)
        echo -e "${BLUE}Building all CDMA executables...${NC}"
        make all
        echo -e "${GREEN}Build successful! Binaries: ./server, ./station, ./simulator, ./test_suite${NC}"
        ;;
    2)
        echo -e "${BLUE}Building and executing CDMA unit test suite...${NC}"
        make test
        ;;
    3)
        echo -e "${BLUE}Starting CDMA Channel Server on port 9090 for N=4 stations...${NC}"
        make server
        ./server -P 9090 -N 4
        ;;
    4)
        echo -e "${BLUE}Launching CDMA Station Client...${NC}"
        make station
        ./station -H 127.0.0.1 -P 9090
        ;;
    5)
        echo -e "${BLUE}Running Automated Multi-Station CDMA Simulation...${NC}"
        make simulator
        ./simulator -N 4 -r 30 -P 9100
        ;;
    6)
        echo -e "${BLUE}Running Simultaneous Text Broadcast Demo over CDMA...${NC}"
        make simulator
        ./simulator --msg-test -P 9105
        ;;
    7)
        echo -e "${BLUE}Executing Noise Immunity & BER Benchmark...${NC}"
        make simulator
        ./simulator --ber-test -P 9110
        ;;
    8)
        echo -e "${YELLOW}Cleaning build artifacts...${NC}"
        make clean
        echo -e "${GREEN}Clean completed.${NC}"
        ;;
    *)
        echo -e "${RED}Invalid option.${NC}"
        exit 1
        ;;
esac
