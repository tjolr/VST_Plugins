# 🎸 Guitar to Bass Plugin - Debugging Guide

## Overview

This guide will help you troubleshoot input source issues with the Guitar to Bass plugin. The plugin now includes comprehensive debug logging to help identify where audio processing might be failing.

## 🔧 Debug Features Added

### 1. Visual Debug Display

- **Debug Label**: Shows real-time status in the plugin UI
- **Click for Details**: Click the debug label to see detailed information
- **Audio Activity Indicators**: Shows IN/NO_IN, OUT/NO_OUT, PITCH/NO_PITCH status

### 2. Console Logging

- **Processor Logs**: All audio processing steps are logged
- **Input/Output Levels**: RMS and peak levels are monitored
- **Pitch Detection**: YIN algorithm status and results
- **Error Detection**: Null pointer checks and processing failures

### 3. Test Input Feature

- **Test Tone Generator**: Click "Test Input" to generate a 440Hz test tone
- **Bypass External Input**: Helps isolate if the issue is with external audio or internal processing

## 🚀 How to Debug

### Step 1: Run the Plugin

```bash
# Build and run the plugin
./build_and_run.sh
```

### Step 2: View Debug Logs

```bash
# Use the debug script
./debug_logs.sh
```

### Step 3: Manual Log Viewing

1. Open **Console.app** (Applications > Utilities > Console)
2. Search for: `GuitarToBass`
3. Run the plugin and watch for logs

## 📊 What to Look For

### Normal Startup Sequence

```
GuitarToBass: GuitarToBassAudioProcessor constructor called
GuitarToBass: Parameter pointers obtained - OctaveShift: OK, SynthMode: OK
GuitarToBass: GuitarToBassAudioProcessor initialization complete
GuitarToBass Editor: Editor constructor called
GuitarToBass Editor: Editor setup complete
```

### Audio Processing Logs

```
GuitarToBass: prepareToPlay called - SampleRate: 44100, SamplesPerBlock: 512
GuitarToBass: Creating YINPitchDetector with analysis size: 1024
GuitarToBass: Creating BassSynthesizer with sample rate: 44100
GuitarToBass: prepareToPlay completed successfully
```

### Real-time Processing (every 1000th block)

```
GuitarToBass: ProcessBlock - InputChannels: 2, OutputChannels: 2, NumSamples: 512
GuitarToBass: Input levels - RMS: 0.000123, MaxSample: 0.000456, HasAudio: YES
GuitarToBass: Synthesis - DetectedPitch: 440.0, CurrentPitch: 440.0, TargetPitch: 220.0
GuitarToBass: Output levels - RMS: 0.000789, MaxSample: 0.001234
```

## 🔍 Troubleshooting Steps

### 1. Check if Audio is Reaching the Plugin

**Look for these logs:**

- `Input levels - RMS: 0.000000, HasAudio: NO` = No audio input
- `Input levels - RMS: 0.000123, HasAudio: YES` = Audio detected

**If no audio:**

- Check DAW input routing
- Verify audio interface settings
- Test with different input sources

### 2. Check Pitch Detection

**Look for these logs:**

- `YIN detection - No pitch found (periodIndex = -1)` = No pitch detected
- `YIN detection - Frequency: 440.0 Hz` = Pitch detected successfully
- `Frequency 500.0 Hz outside guitar range (80-400 Hz)` = Frequency filtered out

**If no pitch detection:**

- Check if input levels are sufficient (> 0.001 RMS)
- Verify guitar is tuned and playing clearly
- Try the "Test Input" button

### 3. Check Output Generation

**Look for these logs:**

- `BassSynthesizer - Frequency: 220.0, Amplitude: 0.300` = Synthesis working
- `Output levels - RMS: 0.000000` = No output generated

**If no output:**

- Check if pitch detection is working
- Verify synth mode settings
- Check amplitude settings

### 4. Use Test Input Feature

1. Click the **"Test Input"** button in the plugin
2. Look for: `Input test mode enabled - generating test tone`
3. Check if input/output levels change
4. This bypasses external audio input

## 🎯 Quick Diagnostic Test

### Test 1: Basic Audio Flow

1. Run plugin
2. Check startup logs (should see constructor and prepareToPlay)
3. Play guitar or use test input
4. Look for input level changes
5. Check for pitch detection messages

### Test 2: Pitch Detection

1. Play a clear guitar note (E2, A2, D3, G3, B3, E4)
2. Look for pitch detection logs
3. Verify frequency is in guitar range (80-400 Hz)

### Test 3: Output Generation

1. Ensure pitch is detected
2. Check synthesis parameters in logs
3. Verify output levels are changing

## 🚨 Common Issues and Solutions

### Issue: No Input Audio

**Symptoms:** `HasAudio: NO` in logs
**Solutions:**

- Check DAW input routing
- Verify audio interface is selected
- Test with different input source
- Use "Test Input" button to verify plugin works

### Issue: No Pitch Detection

**Symptoms:** `No pitch found (periodIndex = -1)`
**Solutions:**

- Increase input gain
- Play clearer, sustained notes
- Check if input levels are sufficient
- Try different guitar strings/notes

### Issue: No Output

**Symptoms:** Output levels remain at 0
**Solutions:**

- Verify pitch detection is working
- Check synth mode settings
- Ensure amplitude is not 0
- Check octave shift settings

### Issue: Plugin Crashes

**Symptoms:** Plugin stops responding
**Solutions:**

- Check for null pointer errors in logs
- Verify buffer sizes are reasonable
- Check for memory allocation issues

## 📝 Log Levels and Frequency

- **Startup**: Once per plugin instance
- **ProcessBlock**: Every 1000th call (to avoid spam)
- **Pitch Detection**: Every 100th call
- **Synthesis**: Every 10000th call
- **Errors**: Every occurrence

## 🔧 Advanced Debugging

### Custom Debug Levels

You can modify the debug frequency by changing these values in the code:

- `processCounter % 1000` - Process block logging frequency
- `debugCounter % 100` - Pitch detection logging frequency
- `synthCounter % 10000` - Synthesis logging frequency

### Adding More Debug Points

To add more debug logging:

1. Add `debugLog("Your message here");` in the processor
2. Add `debugLogEditor("Your message here");` in the editor
3. Rebuild the plugin

## 📞 Getting Help

If you're still having issues:

1. Collect all debug logs
2. Note the specific symptoms
3. Include your system information
4. Describe what you've tried

The debug logs will help identify exactly where the audio processing pipeline is failing!
