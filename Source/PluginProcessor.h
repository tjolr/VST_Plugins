/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>

//==============================================================================
// Bass Synthesizer for generating bass tones
class BassSynthesizer
{
public:
    BassSynthesizer(float sampleRate);
    
    void setFrequency(float frequency);
    void setAmplitude(float amplitude);
    void setSynthMode(bool synthMode); // true = synth, false = analog
    void renderBlock(float* output, int numSamples);
    void reset();
    
private:
    void generateWavetable();
    void generateAnalogWave();
    float getNextSample();
    
    float sampleRate_;
    float frequency_ = 440.0f;
    float amplitude_ = 0.5f;
    bool synthMode_ = true;
    
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
    
private:
    void calculateDifference(const float* audioBuffer, int numSamples);
    void calculateCumulativeMeanNormalizedDifference();
    int getAbsoluteThreshold(float threshold = 0.1f);
    float parabolicInterpolation(int peakIndex);
    void processOverlappingWindow(const float* input, int numSamples);
    void generateWindow();
    
    // Polyphonic detection methods
    std::vector<float> detectMultiplePitches(const float* buffer, int numSamples);
    std::vector<float> findSpectralPeaks(const std::vector<float>& spectrum);
    float findLowestPitch(const std::vector<float>& pitches);
    
    std::vector<float> differenceBuffer_;
    std::vector<float> cumulativeBuffer_;
    int bufferSize_;
    float sampleRate_;
    
    // Overlapping window analysis
    std::vector<float> analysisBuffer_;
    std::vector<float> window_;
    int writeIndex_ = 0;
    int hopSize_;
    bool bufferReady_ = false;
    float lastPitch_ = 0.0f;
    float pitchSmoothing_ = 0.7f;
    
    // FFT for polyphonic analysis
    std::unique_ptr<juce::dsp::FFT> fft_;
    std::vector<float> fftBuffer_;
    std::vector<float> spectrum_;
    static constexpr int fftOrder_ = 10; // 1024 point FFT
    
    static constexpr float minFreq_ = 80.0f;  // Low E string
    static constexpr float maxFreq_ = 400.0f; // Upper guitar range
    static constexpr float overlapRatio_ = 0.75f; // 75% overlap for smooth tracking
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

private:
    //==============================================================================
    std::unique_ptr<YINPitchDetector> pitchDetector_;
    std::unique_ptr<BassSynthesizer> bassSynthesizer_;
    float currentPitch_ = 0.0f;
    
    // Level monitoring
    std::atomic<float> inputLevel_{0.0f};
    std::atomic<float> outputLevel_{0.0f};
    
    // Audio engine state
    double currentSampleRate_ = 44100.0;
    int currentBlockSize_ = 512;
    juce::AudioBuffer<float> inputBuffer_;
    juce::AudioBuffer<float> outputBuffer_;
    
    // Noise gate parameters
    float gateThreshold_ = -50.0f; // dB threshold for noise gate
    float gateRatio_ = 10.0f; // Gate ratio (how much to reduce below threshold)
    float gateAttack_ = 0.01f; // Attack time in seconds
    float gateRelease_ = 0.1f; // Release time in seconds
    float gateLevel_ = 0.0f; // Current gate level (0.0 = closed, 1.0 = open)
    float gateCoeff_ = 0.0f; // Gate coefficient for smoothing
    
    // Parameter management
    juce::AudioProcessorValueTreeState parameters_;
    std::atomic<float>* octaveShiftParam_ = nullptr;
    std::atomic<float>* synthModeParam_ = nullptr;
    std::atomic<float>* inputTestParam_ = nullptr;
    std::atomic<float>* gateThresholdParam_ = nullptr;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarToBassAudioProcessor)
};
