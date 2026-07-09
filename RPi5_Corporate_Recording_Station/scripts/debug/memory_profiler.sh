#!/bin/bash
# memory_profiler.sh - Memory profiling for Recording Station

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║     Memory Profiler                                      ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"

# Check if valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo -e "${RED}❌ Valgrind not found. Install: apt-get install valgrind${NC}"
    exit 1
fi

BINARY=${1:-"/opt/recording-station/recording-station"}
if [ ! -f "$BINARY" ]; then
    echo -e "${RED}❌ Binary not found: $BINARY${NC}"
    exit 1
fi

echo -e "${YELLOW}Profiling: $BINARY${NC}"
echo -e "${YELLOW}Output directory: ./profiling/${NC}"

# Create output directory
mkdir -p profiling

# Run valgrind with memory check
echo -e "\n${YELLOW}[1/3] Running memory check...${NC}"
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=profiling/memcheck.log \
         "$BINARY" --test

echo -e "${GREEN}✓ Memory check complete${NC}"

# Run massif for heap profiling
echo -e "\n${YELLOW}[2/3] Running heap profiling...${NC}"
valgrind --tool=massif \
         --stacks=yes \
         --massif-out-file=profiling/massif.out \
         "$BINARY" --test

echo -e "${GREEN}✓ Heap profiling complete${NC}"

# Generate report
echo -e "\n${YELLOW}[3/3] Generating report...${NC}"
ms_print profiling/massif.out > profiling/massif-report.txt

# Show summary
echo -e "\n${GREEN}━━━ Memory Profile Summary ━━━${NC}"

# Check for memory leaks
if grep -q "definitely lost" profiling/memcheck.log; then
    LEAKS=$(grep "definitely lost" profiling/memcheck.log | head -1)
    echo -e "${RED}✗ Memory leaks detected:${NC}"
    echo "  $LEAKS"
else
    echo -e "${GREEN}✓ No memory leaks detected${NC}"
fi

# Show memory usage
if grep -q "peak" profiling/massif-report.txt; then
    PEAK=$(grep "peak" profiling/massif-report.txt | head -1 | awk '{print $NF}')
    echo -e "Peak heap usage: $PEAK"
fi

echo -e "\n${GREEN}📊 Reports available:${NC}"
echo -e "  profiling/memcheck.log      - Memory leak report"
echo -e "  profiling/massif-report.txt - Heap usage report"
echo -e "  profiling/massif.out        - Raw massif data"

# Open massif report with viewer if available
if command -v massif-visualizer &> /dev/null; then
    massif-visualizer profiling/massif.out &
fi
