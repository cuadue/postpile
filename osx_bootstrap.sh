#!/bin/bash
cd "$(dirname "${BASH_SOURCE[0]}")"
DYLD_LIBRARY_PATH=.:../Frameworks exec ./_postpile
