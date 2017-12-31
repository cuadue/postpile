#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
DYLD_PRINT_LIBRARIES=1 DYLD_FALLBACK_LIBRARY_PATH=. exec ./_postpile
