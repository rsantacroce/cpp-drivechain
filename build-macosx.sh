#!/bin/bash
# Build for unix with bench, fuzz, and tests disabled using all available cores
# Also build Qt GUI 
cmake -B build -DBUILD_GUI=OFF -DBUILD_BENCH=OFF -DBUILD_FUZZ_BINARY=OFF -DBUILD_GUI_TESTS=OFF -DBUILD_TESTS=OFF
cmake --build build -j $(nproc) 
#cmake --build build -j $(nproc)  --clean-first 
