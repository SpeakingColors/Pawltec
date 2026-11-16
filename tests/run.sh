#!/bin/sh
# Build and run the core tests (no JUCE needed).
set -e
cd "$(dirname "$0")/.."
g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/pawltec_test tests/test_core.cpp
/tmp/pawltec_test

# Studio 87 Gear Updater link (product id and link string)
(cd "$(dirname "$0")/.." && g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/s87_link_test tests/test_updater_link.cpp && /tmp/s87_link_test) || exit 1
