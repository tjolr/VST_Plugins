# Guitar-to-Bass VST Plugin Specification

## Plugin Purpose & UX Vision

This VST plugin converts guitar input (including full chords) into bass output by extracting the lowest note and synthesizing a bass tone. Musicians can select octave shifting (0-4 octaves down) and toggle between synthetic and analog-style bass tones. The plugin provides real-time note detection with minimal latency, enabling guitarists to generate bass lines dynamically during performance or recording.

## Core Functional Requirements

- Extract fundamental frequencies from polyphonic guitar input using YIN algorithm
- Identify and isolate the lowest detected note from chord input
- Generate bass tones using wavetable synthesis (synth mode) or analog modeling (analog mode)
- Provide octave shifting from 0 to 4 octaves below detected pitch
- Maintain real-time performance with <10ms latency for note detection
- Handle both monophonic single notes and polyphonic chord input
- Output single monophonic bass note regardless of input complexity
- Provide smooth parameter transitions to avoid audio artifacts

## Technical Architecture Overview

```
[Guitar Input] → [YIN Pitch Detector] → [Lowest Note Extractor] → [Octave Shifter] → [Bass Synthesizer] → [Audio Output]
                                                                                            ↑
                 [GUI Controls] ← [Parameter Manager] ← [Synth/Analog Toggle]
```

**Core DSP Modules:**
- YIN algorithm for robust guitar pitch detection (lower latency than FFT)
- Wavetable oscillator for synthetic bass generation
- Analog modeling filters for analog bass emulation  
- Overlap-add windowing for continuous analysis

**External Dependencies:**
- JUCE framework (juce_dsp, juce_audio_processors, juce_gui_basics)
- Custom YIN implementation for guitar-optimized pitch detection

## Repo & Paper Leverage Matrix

```yaml
- source: repo
  name: JUCE Framework
  url: https://github.com/juce-framework/JUCE
  role_in_project: Core audio framework and plugin wrapper
  reason_for_rank: Industry standard, comprehensive audio/GUI toolkit

- source: paper  
  name: YIN Fundamental Frequency Estimator
  url: http://audition.ens.fr/adc/pdf/2002_JASA_YIN.pdf
  role_in_project: Primary pitch detection algorithm
  reason_for_rank: Proven robust for guitar/musical instrument analysis

- source: repo
  name: JUCE Synthesizer Examples
  url: https://github.com/juce-framework/JUCE/tree/master/examples/Audio
  role_in_project: Synthesis architecture reference
  reason_for_rank: Official JUCE synthesis patterns and best practices

- source: paper
  name: Real-time Pitch Tracking
  url: https://ccrma.stanford.edu/~jos/sasp/Fundamental_Frequency_Estimation.html
  role_in_project: Alternative algorithms for comparison
  reason_for_rank: Comprehensive analysis of pitch detection methods
```

## Build Feasibility & Risk Assessment

| Area | Complexity(1-5) | Risk(1-5) | Mitigation |
|------|----------------|-----------|------------|
| YIN Algorithm Implementation | 4 | 3 | Use proven academic implementation, extensive unit testing |
| Polyphonic Note Extraction | 5 | 4 | Start with monophonic, expand iteratively with spectral analysis |
| Real-time Performance | 3 | 3 | Profile audio thread, optimize buffer sizes, lock-free design |
| Synthesis Quality | 2 | 2 | Leverage JUCE Synthesizer framework, wavetable approach |
| GUI Responsiveness | 2 | 1 | Standard JUCE components, established patterns |
| Cross-platform Build | 2 | 2 | JUCE handles platform abstraction, test on target systems |

## Validation Strategy

**Smoke Build Gate (First Priority):**
1. Compile basic JUCE plugin skeleton with Ninja
2. Run pluginval --strictness high --validate-in-process
3. Process 2s pink noise through bypass mode (RMS diff <0.1dB)
4. Generate UI screenshot validation (>200px width, >100px height)

**DSP Validation Tests:**
- Unit tests for YIN algorithm accuracy using synthetic sine waves
- Chord detection tests with known guitar chord samples
- Latency measurement tests (<10ms requirement)
- CPU usage profiling (must not exceed 90% buffer period)

**Integration Testing:**
- End-to-end audio processing with guitar input samples
- Parameter automation testing for smooth transitions
- Real-time performance validation under DAW hosting

**Custom Test Harness:**
- Python/NumPy analysis for rendered audio validation
- Automated regression testing with reference audio files
- Continuous validation pipeline with known guitar samples

## Open Questions

- Should we implement multi-string polyphonic detection or focus on lowest-note extraction from spectral analysis?
- What specific wavetable content should be used for realistic bass synthesis - sampled bass or mathematically generated?
- How should we handle rapid chord changes - should there be a minimum note duration threshold?
- What GUI framework within JUCE provides the most modern appearance for the octave control and toggle switch?
- Should analog bass mode use physical modeling or simpler filter-based synthesis for real-time performance?