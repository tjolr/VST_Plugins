/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <string>

//==============================================================================
// Instrument modes
enum class InstrumentMode
{
    AnalogBass = 0,
    SynthBass = 1,
    Piano = 2
};

class MultiInstrumentSynthesizer
{
public:
    MultiInstrumentSynthesizer(float sampleRate);
    
    void setFrequency(float frequency);
    void setAmplitude(float amplitude);
    void setInstrumentMode(InstrumentMode mode);
    void renderBlock(float* output, int numSamples);
    void reset();
    
private:
    void generateWavetable();
    void generateAnalogWave();
    void generatePianoWavetable();
    float getNextSample();
    
    [[maybe_unused]] float sampleRate_;
    float frequency_ = 440.0f;
    float amplitude_ = 0.5f;
    InstrumentMode instrumentMode_ = InstrumentMode::SynthBass;
    
    // Wavetable synthesis
    std::vector<float> wavetable_;
    float phase_ = 0.0f;
    float phaseIncrement_ = 0.0f;
    
    // Analog synthesis (simple oscillator + filter)
    float analogPhase_ = 0.0f;
    float lowPassState_ = 0.0f;
    float filterCutoff_ = 0.3f;
    
    // Envelope
    float envelope_ = 0.0f;
    float envelopeTarget_ = 0.0f;
    float envelopeRate_ = 0.01f;
    
    static constexpr int wavetableSize_ = 1024;
};

//==============================================================================
// YIN Algorithm for pitch detection
class YINPitchDetector
{
public:
    YINPitchDetector(int bufferSize, float sampleRate);
    float detectPitch(const float* audioBuffer, int numSamples);
    std::vector<float> detectMultiplePitches(const float* buffer, int numSamples);
    float detectPitchWithConfidence(const float* audioBuffer, int numSamples, float& confidence);
    
private:
    void calculateDifference(const float* audioBuffer, int numSamples);
    void calculateCumulativeMeanNormalizedDifference();
    int getAbsoluteThreshold(float threshold = 0.1f);
    float parabolicInterpolation(int peakIndex);
    
    // Enhanced detection methods
    float calculatePitchConfidence(int periodIndex);
    std::vector<float> preprocessAudio(const float* input, int numSamples);
    bool validatePitchCandidate(float frequency, float confidence);
    void processOverlappingWindow(const float* input, int numSamples);
    void generateWindow();
    
    // Polyphonic detection methods
    std::vector<float> findSpectralPeaks(const std::vector<float>& spectrum);
    float findLowestPitch(const std::vector<float>& pitches);
    
    std::vector<float> differenceBuffer_;
    std::vector<float> cumulativeBuffer_;
    int bufferSize_;
    [[maybe_unused]] float sampleRate_;
    
    // Overlapping window analysis
    std::vector<float> analysisBuffer_;
    std::vector<float> window_;
    int writeIndex_ = 0;
    int hopSize_;
    bool bufferReady_ = false;
    float lastPitch_ = 0.0f;
    float pitchSmoothing_ = 0.85f; // Increased smoothing for stability
    
    // Enhanced pitch detection parameters
    float pitchConfidence_ = 0.0f;
    int consecutiveDetections_ = 0;
    static constexpr int minConsecutiveDetections_ = 3; // Require 3 consecutive detections
    static constexpr float confidenceThreshold_ = 0.6f; // Minimum confidence for valid detection
    
    // FFT for polyphonic analysis
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> fftBuffer_;
    std::vector<float> spectrum_;
    static constexpr int fftOrder_ = 10; // 1024 point FFT
    
    static constexpr float minFreq_ = 70.0f;  // Expanded range for low notes
    static constexpr float maxFreq_ = 500.0f; // Expanded range for higher notes
    static constexpr float overlapRatio_ = 0.75f; // 75% overlap for smooth tracking
};

//==============================================================================
// Note Detection and Chord Analysis Classes
class NoteDetector
{
public:
    struct Note {
        float frequency = 0.0f;
        int midiNote = -1;
        float confidence = 0.0f;
        std::string noteName;
        
        Note() = default;
        Note(float freq, float conf) : frequency(freq), confidence(conf) {
            midiNote = frequencyToMidiNote(freq);
            noteName = midiNoteToNoteName(midiNote);
        }
    };
    
    static int frequencyToMidiNote(float frequency);
    static float midiNoteToFrequency(int midiNote);
    static std::string midiNoteToNoteName(int midiNote);
    static int noteNameToMidiNote(const std::string& noteName);
};

//==============================================================================
// Chord Root Detection with stability delay
class ChordRootDetector
{
public:
    ChordRootDetector(float sampleRate, float stabilityDelayMs = 100.0f);
    
    struct ChordInfo {
        NoteDetector::Note rootNote;
        std::vector<NoteDetector::Note> detectedNotes;
        float confidence = 0.0f;
        bool isStable = false;
        
        bool isValid() const { return rootNote.midiNote >= 0; }
    };
    
    ChordInfo analyzeNotes(const std::vector<float>& frequencies);
    ChordInfo getCurrentChord() const { return currentChord_; }
    void reset();
    
private:
    NoteDetector::Note findChordRoot(const std::vector<NoteDetector::Note>& notes);
    bool isChordStable(const ChordInfo& newChord);
    void updateStabilityBuffer(const ChordInfo& chord);
    
    [[maybe_unused]] float sampleRate_;
    int stabilityDelaySamples_;
    int currentSampleCount_;
    
    ChordInfo currentChord_;
    ChordInfo pendingChord_;
    std::vector<ChordInfo> stabilityBuffer_;
    int bufferWriteIndex_ = 0;
    bool bufferFull_ = false;
    
    static constexpr int stabilityBufferSize_ = 10; // 10 analysis windows for stability
    static constexpr float chordChangeThreshold_ = 2.0f; // semitones
};

//==============================================================================
// Bass Note Mapping to standard bass guitar tuning
class BassNoteMapper
{
public:
    struct BassNote {
        float frequency = 0.0f;
        int midiNote = -1;
        std::string noteName;
        std::string stringName;
        
        bool isValid() const { return midiNote >= 0; }
    };
    
    BassNoteMapper();
    
    BassNote mapToClosestBassNote(int inputMidiNote);
    BassNote mapToClosestBassNote(float inputFrequency);
    BassNote mapChordRootToBass(const NoteDetector::Note& chordRoot);
    
    const std::vector<BassNote>& getBassTuning() const { return bassTuning_; }
    
private:
    std::vector<BassNote> bassTuning_; // E1, A1, D2, G2 (standard bass tuning)
    
    void initializeBassTuning();
    int findClosestBassNoteIndex(int midiNote);
};

//==============================================================================
// Simple real-time pitch detection fallback
float detectPitchSimple(const float* audioBuffer, int numSamples, double sampleRate);

//==============================================================================
/**
*/
class GuitarToBassAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    GuitarToBassAudioProcessor();
    ~GuitarToBassAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    
    //==============================================================================
    juce::AudioProcessorValueTreeState& getParameters() { return parameters_; }
    float getCurrentPitch() const { return currentPitch_; }
    float getInputLevel() const { return inputLevel_; }
    float getOutputLevel() const { return outputLevel_; }
    
    // Note-based detection getters
    ChordRootDetector::ChordInfo getCurrentChord() const;
    BassNoteMapper::BassNote getCurrentBassNote() const;
    std::vector<NoteDetector::Note> getDetectedNotes() const;
    
    // MIDI output functionality
    void generateMidiOutput(juce::MidiBuffer& midiBuffer, int numSamples);
    
    // Harmonic analysis structure
    struct HarmonicInfo {
        float fundamental = 0.0f;
        float confidence = 0.0f;
        std::vector<float> harmonics;
    };
    
    // Note stability methods
    void updateNoteStability(float detectedFrequency, float confidence);
    int quantizeFrequencyToMidiNote(float frequency);
    bool isFrequencyCloseToNote(float frequency, int midiNote, float tolerance = 15.0f);
    void resetNoteStability();
    
    // Enhanced harmonic analysis
    HarmonicInfo analyzeHarmonics(const std::vector<float>& detectedPitches);
    float findBestFundamental(const std::vector<float>& frequencies);
    float calculateHarmonicConfidence(float fundamental, const std::vector<float>& frequencies);
    void updateNoteWithHysteresis(float newFrequency, float confidence);

private:
    //==============================================================================
    std::unique_ptr<YINPitchDetector> pitchDetector_;
    std::unique_ptr<MultiInstrumentSynthesizer> instrumentSynthesizer_;
    std::unique_ptr<ChordRootDetector> chordDetector_;
    std::unique_ptr<BassNoteMapper> bassMapper_;
    float currentPitch_ = 0.0f;
    
    // Note-based detection state
    std::vector<NoteDetector::Note> currentDetectedNotes_;
    ChordRootDetector::ChordInfo currentChord_;
    BassNoteMapper::BassNote currentBassNote_;
    
    // Level monitoring
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};
    
    // Audio engine state
    double currentSampleRate_ = 44100.0;
    int currentBlockSize_ = 512;
    juce::AudioBuffer<float> inputBuffer_;
    juce::AudioBuffer<float> outputBuffer_;
    
    // Noise gate parameters
    [[maybe_unused]] float gateThreshold_ = -50.0f; // dB threshold for noise gate
    [[maybe_unused]] float gateRatio_ = 10.0f; // Gate ratio (how much to reduce below threshold)
    float gateAttack_ = 0.01f; // Attack time in seconds
    float gateRelease_ = 0.1f; // Release time in seconds
    float gateLevel_ = 0.0f; // Current gate level (0.0 = closed, 1.0 = open)
    [[maybe_unused]] float gateCoeff_ = 0.0f; // Gate coefficient for smoothing
    
    // Parameter management
    juce::AudioProcessorValueTreeState parameters_;
    std::atomic<float>* octaveShiftParam_ = nullptr;
    std::atomic<float>* instrumentModeParam_ = nullptr;
    std::atomic<float>* inputTestParam_ = nullptr;
    std::atomic<float>* gateThresholdParam_ = nullptr;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // MIDI output state
    int currentMidiNote_ = -1;  // Currently playing MIDI note (-1 = no note)
    bool midiNoteOn_ = false;   // Whether a MIDI note is currently on
    float midiNoteVelocity_ = 0.0f; // Current note velocity (0-127)
    
    // Note stability and smoothing
    struct NoteCandidate {
        int midiNote = -1;
        float frequency = 0.0f;
        float confidence = 0.0f;
        int consecutiveCount = 0;
        float averageFrequency = 0.0f;
    };
    
    std::vector<NoteCandidate> noteCandidates_;
    int stableNote_ = -1;  // Currently stable note (-1 = no stable note)
    float stableNoteFrequency_ = 0.0f;
    int stableNoteConfirmationCount_ = 0;
    static constexpr int minConfirmationCount_ = 5;  // Reduced from 8 to 5 for faster lock-on
    static constexpr float maxFrequencyDeviation_ = 15.0f; // Increased from 10 to 15 Hz tolerance
    static constexpr float confidenceThreshold_ = 0.3f; // Reduced from 0.5 to 0.3 for more sensitivity
    
    // Enhanced stability features
    int noteHoldCount_ = 0;  // How long to hold a note after detection stops
    static constexpr int maxNoteHoldCount_ = 20; // Hold note for 20 processing cycles
    float noteHysteresis_ = 0.8f; // Hysteresis factor for note switching
    
    // Harmonic analysis for better fundamental detection
    HarmonicInfo lastHarmonicAnalysis_;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarToBassAudioProcessor)
};
