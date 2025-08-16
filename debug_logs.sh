#!/bin/bash

echo "🎸 Guitar to Bass Debug Log Viewer"
echo "=================================="
echo ""
echo "This script will show debug logs in real-time."
echo "Make sure the plugin is running in your DAW or standalone app."
echo ""
echo "Press Ctrl+C to stop viewing logs."
echo ""

# Function to show logs with timestamps
show_logs() {
    echo "📊 Real-time debug logs:"
    echo "========================"
    
    # Use log stream to get real-time logs
    log stream --predicate 'process == "Guitar to Bass" OR process == "GuitarToBassPlugin" OR messageType == 16' --style compact | grep -E "(GuitarToBass|Guitar to Bass)" || {
        echo "No logs found. Try running the plugin first."
        echo ""
        echo "Alternative: Check Console.app for logs:"
        echo "1. Open Console.app"
        echo "2. Search for 'GuitarToBass'"
        echo "3. Run the plugin and watch for logs"
    }
}

# Check if log command is available
if command -v log &> /dev/null; then
    show_logs
else
    echo "❌ 'log' command not available on this system."
    echo ""
    echo "📋 Manual debugging steps:"
    echo "1. Open Console.app (Applications > Utilities > Console)"
    echo "2. In the search bar, type: GuitarToBass"
    echo "3. Run your plugin (standalone or in DAW)"
    echo "4. Watch for debug messages like:"
    echo "   - GuitarToBass: GuitarToBassAudioProcessor constructor called"
    echo "   - GuitarToBass: prepareToPlay called"
    echo "   - GuitarToBass: ProcessBlock - InputChannels: 2"
    echo "   - GuitarToBass: Input levels - RMS: 0.000123"
    echo ""
    echo "🔍 What to look for:"
    echo "- Input levels (should be > 0.001 when playing guitar)"
    echo "- Pitch detection messages"
    echo "- Processing errors or warnings"
    echo ""
    echo "🎯 Quick test:"
    echo "1. Run the plugin"
    echo "2. Click the 'Test Input' button"
    echo "3. Check if you see 'Input test mode enabled' in logs"
    echo "4. Check if input/output levels change"
fi