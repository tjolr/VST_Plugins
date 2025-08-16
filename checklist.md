# Guitar-to-Bass VST Plugin Implementation Checklist

## SmokeBuild Phase

- [ ] **SB1** Configure JUCE project for host CPU architecture
  - **objective**: Set up build system for local development without universal binary
  - **tests**: tests/build_config_test.cpp
  - **implementation_hint**: Modify CMakeLists.txt to target host architecture only
  - **done_when**: Project compiles successfully with Ninja generator

- [ ] **SB2** Create basic plugin skeleton with bypass functionality  
  - **objective**: Establish minimal VST3/AU plugin that passes audio through unchanged
  - **tests**: tests/bypass_test.cpp
  - **implementation_hint**: Use JUCE AudioProcessor template with processBlock implementation
  - **done_when**: Plugin loads in DAW and passes audio without modification

- [ ] **SB3** Validate plugin with pluginval stress testing
  - **objective**: Ensure plugin meets DAW compatibility standards
  - **tests**: Run pluginval --strictness high --validate-in-process
  - **implementation_hint**: Fix any threading, parameter, or lifecycle issues reported
  - **done_when**: pluginval passes all tests without errors or warnings

- [ ] **SB4** Implement pink noise validation and UI screenshot test
  - **objective**: Verify audio processing chain and basic UI rendering
  - **tests**: tests/pink_noise_test.cpp, UI screenshot validation
  - **implementation_hint**: Process 2s pink noise, check RMS difference <0.1dB, capture UI
  - **done_when**: RMS validation passes and UI screenshot >200x100 pixels

## Env Phase

- [ ] **E1** Set up parameter management system
  - **objective**: Create AudioProcessorValueTreeState for plugin parameters
  - **tests**: tests/parameter_test.cpp
  - **implementation_hint**: Define octave shift (0-4) and synth/analog toggle parameters
  - **done_when**: Parameters persist across plugin instances and respond to automation

- [ ] **E2** Configure audio buffer management for real-time processing
  - **objective**: Establish proper audio buffer handling for pitch detection
  - **tests**: tests/buffer_management_test.cpp  
  - **implementation_hint**: Use circular buffers for overlapping analysis windows
  - **done_when**: Audio buffers handle various host buffer sizes without artifacts

## DSP Phase

- [ ] **D1** Implement YIN pitch detection algorithm core
  - **objective**: Create fundamental frequency estimator optimized for guitar input
  - **tests**: tests/yin_algorithm_test.cpp
  - **implementation_hint**: Use autocorrelation with threshold-based fundamental detection
  - **done_when**: Correctly detects pitch of synthetic sine waves within 5 cents accuracy

- [ ] **D2** Add overlapping window analysis for continuous pitch tracking
  - **objective**: Enable smooth pitch detection without frame boundary artifacts  
  - **tests**: tests/windowing_test.cpp
  - **implementation_hint**: Implement Hann windowing with 50% overlap for YIN analysis
  - **done_when**: Pitch detection works continuously on sustained guitar notes

- [ ] **D3** Implement polyphonic note detection and lowest note extraction
  - **objective**: Identify multiple frequencies and select fundamental bass note
  - **tests**: tests/polyphonic_detection_test.cpp
  - **implementation_hint**: Use spectral peak picking to find multiple fundamentals
  - **done_when**: Correctly identifies lowest note from known guitar chord samples

- [ ] **D4** Create octave shifting processor  
  - **objective**: Transpose detected pitch by 0-4 octaves down
  - **tests**: tests/octave_shift_test.cpp
  - **implementation_hint**: Multiply frequency by 2^(-octaves) for pitch shifting
  - **done_when**: Output frequency matches expected octave-shifted target frequency

- [ ] **D5** Implement wavetable bass synthesizer core
  - **objective**: Generate synthetic bass tones using wavetable synthesis
  - **tests**: tests/wavetable_synth_test.cpp
  - **implementation_hint**: Use JUCE SynthesiserVoice with custom wavetable oscillator
  - **done_when**: Produces clean bass tones at specified frequencies

- [ ] **D6** Add analog bass synthesis mode
  - **objective**: Create alternative analog-style bass tone generation
  - **tests**: tests/analog_bass_test.cpp
  - **implementation_hint**: Use filtered sawtooth/square waves with ADSR envelope
  - **done_when**: Generates distinct analog-character bass tones

- [ ] **D7** Implement smooth parameter transitions
  - **objective**: Prevent audio artifacts during parameter changes
  - **tests**: tests/parameter_smoothing_test.cpp
  - **implementation_hint**: Use linear interpolation for octave changes, crossfading for mode switch
  - **done_when**: No clicks or pops during parameter automation

- [ ] **D8** Optimize real-time performance for <10ms latency
  - **objective**: Ensure processing meets real-time constraints
  - **tests**: tests/latency_test.cpp
  - **implementation_hint**: Profile audio thread, minimize allocations, use lock-free design
  - **done_when**: Audio processing completes within 90% of buffer period

## UI Phase

- [ ] **U1** Create custom LookAndFeel for modern appearance
  - **objective**: Design contemporary plugin interface styling
  - **tests**: tests/lookandfeel_test.cpp
  - **implementation_hint**: Derive from LookAndFeel_V4 with custom colors and component styles
  - **done_when**: UI components display with consistent modern styling

- [ ] **U2** Implement octave control slider
  - **objective**: Provide 0-4 octave down selection with visual feedback
  - **tests**: tests/octave_slider_test.cpp
  - **implementation_hint**: Use Slider with custom textFromValue and valueFromText
  - **done_when**: Slider accurately controls octave parameter with proper labeling

- [ ] **U3** Add synth/analog bass toggle switch
  - **objective**: Switch between synthesis modes with clear visual state
  - **tests**: tests/bass_toggle_test.cpp
  - **implementation_hint**: Use ToggleButton with custom paintButton implementation
  - **done_when**: Toggle correctly switches bass synthesis mode with visual indication

- [ ] **U4** Create responsive layout using FlexBox
  - **objective**: Ensure UI adapts to different plugin window sizes
  - **tests**: tests/layout_test.cpp
  - **implementation_hint**: Use FlexBox for main layout with 8px padding, 16px spacing
  - **done_when**: Components resize appropriately at various window dimensions

- [ ] **U5** Add real-time pitch display visualization
  - **objective**: Show detected pitch and bass note output to user
  - **tests**: tests/pitch_display_test.cpp
  - **implementation_hint**: Use Timer callback to update pitch text display at 30Hz
  - **done_when**: Display shows current detected and output pitches accurately

- [ ] **U6** Implement tooltip help for all controls
  - **objective**: Provide user guidance for plugin operation
  - **tests**: tests/tooltip_test.cpp
  - **implementation_hint**: Add TooltipWindow and setTooltip calls for each component
  - **done_when**: All interactive elements show helpful tooltips on hover

## Integration Phase

- [ ] **I1** Connect DSP processing to GUI parameters
  - **objective**: Ensure parameter changes affect audio processing correctly
  - **tests**: tests/dsp_gui_integration_test.cpp
  - **implementation_hint**: Use AudioProcessorValueTreeState::Listener for parameter updates
  - **done_when**: GUI parameter changes immediately affect audio output

- [ ] **I2** Run pluginval validation test
  - **objective**: Verify plugin passes all DAW compatibility tests
  - **tests**: pluginval --strictness high --validate-in-process
  - **implementation_hint**: Address any threading, automation, or state issues
  - **done_when**: pluginval completes without errors

- [ ] **I3** Execute offline audio render regression test
  - **objective**: Validate consistent audio processing behavior
  - **tests**: tests/render_regression_test.cpp
  - **implementation_hint**: Process known guitar samples, compare with reference output
  - **done_when**: Rendered audio matches expected DSP behavior within tolerance

- [ ] **I4** Perform real-time CPU and glitch benchmark
  - **objective**: Ensure plugin meets performance requirements under load
  - **tests**: tests/cpu_benchmark_test.cpp
  - **implementation_hint**: Measure processBlock execution time under various conditions
  - **done_when**: CPU usage stays below 90% of buffer period, no audio dropouts

- [ ] **I5** Validate UI screenshot sanity check
  - **objective**: Confirm GUI renders correctly for automated testing
  - **tests**: juce_pluginhost --headless --capture-ui
  - **implementation_hint**: Ensure plugin UI displays properly in headless mode
  - **done_when**: Screenshot dimensions exceed 200x100 pixels with visible controls

## Packaging Phase

- [ ] **P1** Copy plugin binaries to system paths
  - **objective**: Install plugin for testing in DAW environment
  - **tests**: Verify plugin loads in host DAW
  - **implementation_hint**: Copy to ~/Library/Audio/Plug-Ins/VST3/ and Components/
  - **done_when**: Plugin appears in DAW plugin list and loads successfully

- [ ] **P2** Create user documentation
  - **objective**: Provide basic usage instructions for guitarists
  - **tests**: Documentation review and accuracy check
  - **implementation_hint**: Document octave control, synthesis modes, and setup
  - **done_when**: Clear instructions enable user to operate plugin effectively

- [ ] **P3** Final validation with complete test suite
  - **objective**: Run all tests to ensure no regressions before completion
  - **tests**: Execute full test suite including all unit and integration tests
  - **implementation_hint**: Address any test failures before marking complete
  - **done_when**: All tests pass consistently across multiple runs