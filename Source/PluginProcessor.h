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
    
    std::vector<float> differenceBuffer_;
    std::vector<float> cumulativeBuffer_;
    int bufferSize_;
    float sampleRate_;
    static constexpr float minFreq_ = 80.0f;  // Low E string
    static constexpr float maxFreq_ = 400.0f; // Upper guitar range
};

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

private:
    //==============================================================================
    std::unique_ptr<YINPitchDetector> pitchDetector_;
    float currentPitch_ = 0.0f;
    
    // Parameter management
    juce::AudioProcessorValueTreeState parameters_;
    std::atomic<float>* octaveShiftParam_ = nullptr;
    std::atomic<float>* synthModeParam_ = nullptr;
    
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarToBassAudioProcessor)
};
