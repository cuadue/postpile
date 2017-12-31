#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
pwd
DYLD_PRINT_LIBRARIES=1 DYLD_FALLBACK_LIBRARY_PATH=. exec ./_postpile
