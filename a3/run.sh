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

echo -e "${BOLD}${CYAN}========================================================${NC}"
echo -e "${BOLD}${CYAN}  CN Lab Assignment 3: CSMA & CSMA/CD Protocol Suite    ${NC}"
echo -e "${BOLD}${CYAN}========================================================${NC}"
echo -e "1) Build All Executables (make all)"
echo -e "2) Run Unit Test Suite (make test)"
echo -e "3) Launch Shared Channel Emulator (Server)"
echo -e "4) Launch Interactive Station Client"
echo -e "5) Run Concurrent CSMA Simulation Demo (Multi-station)"
echo -e "6) Run Automated Benchmark Suite & Generate Vector Plots"
echo -e "7) Clean Build Artifacts (make clean)"
echo -e "--------------------------------------------------------"
read -p "Select an option [1-7]: " opt

case "$opt" in
    1)
        echo -e "${BLUE}Building executables...${NC}"
        make all
        echo -e "${GREEN}Build successful! Binaries: ./server, ./station, ./simulator${NC}"
        ;;
    2)
        echo -e "${BLUE}Building and executing unit tests...${NC}"
        make test
        ;;
    3)
        echo -e "${BLUE}Starting Shared Channel Server...${NC}"
        ./server
        ;;
    4)
        echo -e "${BLUE}Starting Station Client...${NC}"
        ./station
        ;;
    5)
        echo -e "${BLUE}Running Multi-Station Simulation Demo...${NC}"
        make simulator
        echo -e "\n${YELLOW}--- 1. Non-Persistent CSMA (N=5) ---${NC}"
        ./simulator -p np -N 5 -n 10
        echo -e "\n${YELLOW}--- 2. 1-Persistent CSMA (N=5) ---${NC}"
        ./simulator -p 1p -N 5 -n 10
        echo -e "\n${YELLOW}--- 3. p-Persistent CSMA (p=0.2, N=5) ---${NC}"
        ./simulator -p pp --prob 0.2 -N 5 -n 10
        echo -e "\n${YELLOW}--- 4. CSMA/CD with Jam & BEB (N=5) ---${NC}"
        ./simulator -p cd -N 5 -n 10
        ;;
    6)
        echo -e "${BLUE}Executing automated benchmarks and generating plots...${NC}"
        python3 eval/benchmark.py
        python3 eval/plot_metrics.py
        echo -e "${GREEN}Benchmark complete! Results and plots saved to eval/results/${NC}"
        ;;
    7)
        echo -e "${YELLOW}Cleaning build artifacts...${NC}"
        make clean
        echo -e "${GREEN}Clean completed.${NC}"
        ;;
    *)
        echo -e "${RED}Invalid option.${NC}"
        exit 1
        ;;
esac
