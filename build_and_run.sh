#!/bin/bash

# Guitar-to-Bass VST Plugin Build and Run Script
echo "🎸 Building Guitar-to-Bass Plugin..."

# Clean and rebuild
rm -rf build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Build standalone specifically
ninja -C build GuitarToBassPlugin_Standalone

echo "✅ Build complete! Opening standalone app..."
open "build/GuitarToBassPlugin_artefacts/Debug/Standalone/Guitar to Bass.app"

echo "🎵 Plugin should now be running with latest changes!" 