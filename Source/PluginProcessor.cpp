/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

//==============================================================================
// YIN Algorithm Implementation
YINPitchDetector::YINPitchDetector(int bufferSize, float sampleRate)
    : bufferSize_(bufferSize), sampleRate_(sampleRate)
{
    differenceBuffer_.resize(bufferSize / 2);
    cumulativeBuffer_.resize(bufferSize / 2);
}

float YINPitchDetector::detectPitch(const float* audioBuffer, int numSamples)
{
    if (numSamples < bufferSize_ / 2)
        return 0.0f;
        
    calculateDifference(audioBuffer, numSamples);
    calculateCumulativeMeanNormalizedDifference();
    
    int periodIndex = getAbsoluteThreshold(0.1f);
    if (periodIndex == -1)
        return 0.0f;
        
    float refinedPeriod = parabolicInterpolation(periodIndex);
    if (refinedPeriod <= 0.0f)
        return 0.0f;
        
    float frequency = sampleRate_ / refinedPeriod;
    
    // Filter out frequencies outside guitar range
    if (frequency < minFreq_ || frequency > maxFreq_)
        return 0.0f;
        
    return frequency;
}

void YINPitchDetector::calculateDifference(const float* audioBuffer, int numSamples)
{
    int maxTau = std::min(bufferSize_ / 2, numSamples / 2);
    
    for (int tau = 0; tau < maxTau; ++tau)
    {
        float sum = 0.0f;
        for (int j = 0; j < maxTau; ++j)
        {
            float delta = audioBuffer[j] - audioBuffer[j + tau];
            sum += delta * delta;
        }
        differenceBuffer_[tau] = sum;
    }
}

void YINPitchDetector::calculateCumulativeMeanNormalizedDifference()
{
    cumulativeBuffer_[0] = 1.0f;
    float runningSum = 0.0f;
    
    for (int tau = 1; tau < static_cast<int>(differenceBuffer_.size()); ++tau)
    {
        runningSum += differenceBuffer_[tau];
        cumulativeBuffer_[tau] = differenceBuffer_[tau] / (runningSum / tau);
    }
}

int YINPitchDetector::getAbsoluteThreshold(float threshold)
{
    // Start from tau=2 to avoid the trivial minimum at tau=0
    for (int tau = 2; tau < static_cast<int>(cumulativeBuffer_.size()) - 1; ++tau)
    {
        if (cumulativeBuffer_[tau] < threshold)
        {
            // Look for local minimum
            while (tau + 1 < static_cast<int>(cumulativeBuffer_.size()) && 
                   cumulativeBuffer_[tau + 1] < cumulativeBuffer_[tau])
            {
                tau++;
            }
            return tau;
        }
    }
    return -1;
}

float YINPitchDetector::parabolicInterpolation(int peakIndex)
{
    if (peakIndex <= 0 || peakIndex >= static_cast<int>(cumulativeBuffer_.size()) - 1)
        return static_cast<float>(peakIndex);
        
    float y1 = cumulativeBuffer_[peakIndex - 1];
    float y2 = cumulativeBuffer_[peakIndex];
    float y3 = cumulativeBuffer_[peakIndex + 1];
    
    float x0 = (y3 - y1) / (2.0f * (2.0f * y2 - y1 - y3));
    
    return static_cast<float>(peakIndex) + x0;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout GuitarToBassAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> parameters;
    
    // Octave shift parameter (0 to 4 octaves down)
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        "octaveShift",
        "Octave Shift", 
        juce::NormalisableRange<float>(0.0f, 4.0f, 1.0f), 
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("octaves").withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value)) + " oct"; })));
    
    // Synth mode parameter (0 = analog, 1 = synth)
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        "synthMode",
        "Bass Mode",
        true,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction(
            [](bool value, int) { return value ? "Synth" : "Analog"; })));
    
    return { parameters.begin(), parameters.end() };
}

//==============================================================================
GuitarToBassAudioProcessor::GuitarToBassAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), parameters_(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    // Get parameter pointers for real-time access
    octaveShiftParam_ = parameters_.getRawParameterValue("octaveShift");
    synthModeParam_ = parameters_.getRawParameterValue("synthMode");
}

GuitarToBassAudioProcessor::~GuitarToBassAudioProcessor()
{
}

//==============================================================================
const juce::String GuitarToBassAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GuitarToBassAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool GuitarToBassAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool GuitarToBassAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double GuitarToBassAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int GuitarToBassAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int GuitarToBassAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GuitarToBassAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String GuitarToBassAudioProcessor::getProgramName (int index)
{
    return {};
}

void GuitarToBassAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GuitarToBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize YIN pitch detector with appropriate buffer size
    int pitchAnalysisSize = std::max(1024, samplesPerBlock * 2);
    pitchDetector_ = std::make_unique<YINPitchDetector>(pitchAnalysisSize, static_cast<float>(sampleRate));
}

void GuitarToBassAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GuitarToBassAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void GuitarToBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Perform pitch detection on the first input channel
    if (totalNumInputChannels > 0 && pitchDetector_)
    {
        auto* inputData = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();
        
        // Detect pitch using YIN algorithm
        float detectedPitch = pitchDetector_->detectPitch(inputData, numSamples);
        
        // Update current pitch (with basic smoothing)
        if (detectedPitch > 0.0f)
        {
            currentPitch_ = currentPitch_ * 0.8f + detectedPitch * 0.2f;
            
            // Apply octave shifting
            float octaveShift = octaveShiftParam_ ? octaveShiftParam_->load() : 1.0f;
            float targetPitch = currentPitch_ / std::pow(2.0f, octaveShift);
            
            // Check synth mode
            bool synthMode = synthModeParam_ ? synthModeParam_->load() > 0.5f : true;
            
            // TODO: Generate bass tone based on targetPitch and synthMode
            // For now, just store these values for later synthesis implementation
        }
        
        // For now, just pass through the input (we'll add synthesis later)
        for (int channel = 0; channel < std::min(totalNumInputChannels, totalNumOutputChannels); ++channel)
        {
            if (channel != 0)
                buffer.copyFrom(channel, 0, buffer, 0, 0, numSamples);
        }
    }
}

//==============================================================================
bool GuitarToBassAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GuitarToBassAudioProcessor::createEditor()
{
    return new GuitarToBassAudioProcessorEditor (*this);
}

//==============================================================================
void GuitarToBassAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters_.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void GuitarToBassAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(parameters_.state.getType()))
        {
            parameters_.replaceState(juce::ValueTree::fromXml(*xmlState));
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarToBassAudioProcessor();
}
