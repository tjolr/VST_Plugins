/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>
#include <cstring>

//==============================================================================
// YIN Algorithm Implementation
YINPitchDetector::YINPitchDetector(int bufferSize, float sampleRate)
    : bufferSize_(bufferSize), sampleRate_(sampleRate)
{
    differenceBuffer_.resize(bufferSize / 2);
    cumulativeBuffer_.resize(bufferSize / 2);
    
    // Set up overlapping window analysis
    analysisBuffer_.resize(bufferSize);
    window_.resize(bufferSize);
    hopSize_ = static_cast<int>(bufferSize * (1.0f - overlapRatio_));
    generateWindow();
    
    // Set up FFT for polyphonic analysis
    fft_ = std::make_unique<juce::dsp::FFT>(fftOrder_);
    int fftSize = 1 << fftOrder_;
    fftBuffer_.resize(fftSize * 2); // Complex numbers need 2x space
    spectrum_.resize(fftSize / 2); // Only need positive frequencies
}

float YINPitchDetector::detectPitch(const float* audioBuffer, int numSamples)
{
    // Process input through overlapping window system
    processOverlappingWindow(audioBuffer, numSamples);
    
    // Only run analysis when we have a full buffer
    if (!bufferReady_)
        return lastPitch_;
        
    // Try polyphonic detection first for better chord handling
    auto multiplePitches = detectMultiplePitches(analysisBuffer_.data(), bufferSize_);
    float frequency = 0.0f;
    
    if (!multiplePitches.empty())
    {
        // Use lowest detected pitch for bass
        frequency = findLowestPitch(multiplePitches);
    }
    else
    {
        // Fall back to YIN for monophonic detection
        std::vector<float> windowedBuffer(bufferSize_);
        for (int i = 0; i < bufferSize_; ++i)
        {
            windowedBuffer[i] = analysisBuffer_[i] * window_[i];
        }
        
        calculateDifference(windowedBuffer.data(), bufferSize_);
        calculateCumulativeMeanNormalizedDifference();
        
        int periodIndex = getAbsoluteThreshold(0.1f);
        if (periodIndex != -1)
        {
            float refinedPeriod = parabolicInterpolation(periodIndex);
            if (refinedPeriod > 0.0f)
            {
                frequency = sampleRate_ / refinedPeriod;
            }
        }
    }
    
    // Filter out frequencies outside guitar range
    if (frequency < minFreq_ || frequency > maxFreq_)
        return lastPitch_;
    
    // Apply smoothing to reduce jitter
    if (frequency > 0.0f)
    {
        if (lastPitch_ > 0.0f)
        {
            lastPitch_ = lastPitch_ * pitchSmoothing_ + frequency * (1.0f - pitchSmoothing_);
        }
        else
        {
            lastPitch_ = frequency;
        }
    }
        
    return lastPitch_;
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

void YINPitchDetector::processOverlappingWindow(const float* input, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        analysisBuffer_[writeIndex_] = input[i];
        writeIndex_++;
        
        // Check if we have a full buffer for analysis
        if (writeIndex_ >= hopSize_)
        {
            bufferReady_ = true;
            writeIndex_ = 0;
            
            // Shift buffer for overlap
            std::memmove(analysisBuffer_.data(), 
                        analysisBuffer_.data() + hopSize_, 
                        (bufferSize_ - hopSize_) * sizeof(float));
            writeIndex_ = bufferSize_ - hopSize_;
        }
    }
}

void YINPitchDetector::generateWindow()
{
    // Generate Hann window for smooth overlapping
    for (int i = 0; i < bufferSize_; ++i)
    {
        float x = static_cast<float>(i) / (bufferSize_ - 1);
        window_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * x));
    }
}

std::vector<float> YINPitchDetector::detectMultiplePitches(const float* buffer, int numSamples)
{
    int fftSize = 1 << fftOrder_;
    
    // Copy input to FFT buffer (with zero padding if needed)
    std::fill(fftBuffer_.begin(), fftBuffer_.end(), 0.0f);
    int copySize = std::min(numSamples, fftSize);
    
    for (int i = 0; i < copySize; ++i)
    {
        fftBuffer_[i * 2] = buffer[i] * window_[i]; // Real part
        fftBuffer_[i * 2 + 1] = 0.0f; // Imaginary part
    }
    
    // Perform FFT
    fft_->performFrequencyOnlyForwardTransform(fftBuffer_.data());
    
    // Calculate magnitude spectrum
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float real = fftBuffer_[i * 2];
        float imag = fftBuffer_[i * 2 + 1];
        spectrum_[i] = std::sqrt(real * real + imag * imag);
    }
    
    // Find spectral peaks that correspond to musical pitches
    return findSpectralPeaks(spectrum_);
}

std::vector<float> YINPitchDetector::findSpectralPeaks(const std::vector<float>& spectrum)
{
    std::vector<float> pitches;
    int fftSize = 1 << fftOrder_;
    float binWidth = sampleRate_ / fftSize;
    
    // Find peaks in the spectrum
    for (int i = 2; i < static_cast<int>(spectrum.size()) - 2; ++i)
    {
        float frequency = i * binWidth;
        
        // Only consider frequencies in guitar range
        if (frequency < minFreq_ || frequency > maxFreq_)
            continue;
            
        // Check if this is a local maximum
        if (spectrum[i] > spectrum[i-1] && spectrum[i] > spectrum[i+1] &&
            spectrum[i] > spectrum[i-2] && spectrum[i] > spectrum[i+2])
        {
            // Require minimum amplitude threshold
            float maxSpectrum = *std::max_element(spectrum.begin(), spectrum.end());
            if (spectrum[i] > maxSpectrum * 0.1f) // 10% of max
            {
                pitches.push_back(frequency);
            }
        }
    }
    
    return pitches;
}

float YINPitchDetector::findLowestPitch(const std::vector<float>& pitches)
{
    if (pitches.empty())
        return 0.0f;
        
    return *std::min_element(pitches.begin(), pitches.end());
}

//==============================================================================
// Bass Synthesizer Implementation
BassSynthesizer::BassSynthesizer(float sampleRate)
    : sampleRate_(sampleRate)
{
    wavetable_.resize(wavetableSize_);
    generateWavetable();
}

void BassSynthesizer::setFrequency(float frequency)
{
    frequency_ = frequency;
    phaseIncrement_ = frequency_ / sampleRate_;
    
    // Update envelope target when frequency changes (note on)
    if (frequency_ > 0.0f)
    {
        envelopeTarget_ = 1.0f;
    }
    else
    {
        envelopeTarget_ = 0.0f;
    }
}

void BassSynthesizer::setAmplitude(float amplitude)
{
    amplitude_ = juce::jlimit(0.0f, 1.0f, amplitude);
}

void BassSynthesizer::setSynthMode(bool synthMode)
{
    if (synthMode_ != synthMode)
    {
        synthMode_ = synthMode;
        if (synthMode_)
        {
            generateWavetable();
        }
    }
}

void BassSynthesizer::renderBlock(float* output, int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        // Update envelope
        envelope_ += (envelopeTarget_ - envelope_) * envelopeRate_;
        
        // Generate sample
        float sample = getNextSample();
        
        // Apply envelope and amplitude
        output[i] = sample * envelope_ * amplitude_;
        
        // Advance phase
        if (synthMode_)
        {
            phase_ += phaseIncrement_;
            if (phase_ >= 1.0f)
                phase_ -= 1.0f;
        }
        else
        {
            analogPhase_ += phaseIncrement_ * 2.0f * M_PI;
            if (analogPhase_ >= 2.0f * M_PI)
                analogPhase_ -= 2.0f * M_PI;
        }
    }
}

void BassSynthesizer::reset()
{
    phase_ = 0.0f;
    analogPhase_ = 0.0f;
    envelope_ = 0.0f;
    lowPassState_ = 0.0f;
}

void BassSynthesizer::generateWavetable()
{
    // Generate a bass-rich wavetable with fundamental + harmonics
    for (int i = 0; i < wavetableSize_; ++i)
    {
        float x = static_cast<float>(i) / wavetableSize_;
        float angle = 2.0f * M_PI * x;
        
        // Fundamental + some harmonics for bass character
        float sample = std::sin(angle)                    // Fundamental
                     + 0.3f * std::sin(2.0f * angle)     // 2nd harmonic
                     + 0.15f * std::sin(3.0f * angle)    // 3rd harmonic
                     + 0.1f * std::sin(4.0f * angle);    // 4th harmonic
        
        wavetable_[i] = sample * 0.5f; // Scale down
    }
}

float BassSynthesizer::getNextSample()
{
    if (synthMode_)
    {
        // Wavetable synthesis with linear interpolation
        float floatIndex = phase_ * wavetableSize_;
        int index1 = static_cast<int>(floatIndex);
        int index2 = (index1 + 1) % wavetableSize_;
        float frac = floatIndex - index1;
        
        return wavetable_[index1] * (1.0f - frac) + wavetable_[index2] * frac;
    }
    else
    {
        // Analog-style synthesis: sawtooth + lowpass filter
        float sawtooth = (analogPhase_ / M_PI) - 1.0f; // -1 to 1 sawtooth
        
        // Simple one-pole lowpass filter
        lowPassState_ += (sawtooth - lowPassState_) * filterCutoff_;
        
        return lowPassState_ * 0.5f;
    }
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
    
    // Initialize bass synthesizer
    bassSynthesizer_ = std::make_unique<BassSynthesizer>(static_cast<float>(sampleRate));
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

    // Perform pitch detection and bass synthesis
    if (totalNumInputChannels > 0 && pitchDetector_ && bassSynthesizer_)
    {
        auto* inputData = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();
        
        // Calculate input level (RMS)
        float inputRMS = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            inputRMS += inputData[i] * inputData[i];
        }
        inputRMS = std::sqrt(inputRMS / numSamples);
        inputLevel_.store(inputRMS);
        
        // Detect pitch using YIN algorithm
        float detectedPitch = pitchDetector_->detectPitch(inputData, numSamples);
        
        // Update current pitch (with basic smoothing)
        if (detectedPitch > 0.0f)
        {
            currentPitch_ = currentPitch_ * 0.8f + detectedPitch * 0.2f;
        }
        
        // Apply octave shifting
        float octaveShift = octaveShiftParam_ ? octaveShiftParam_->load() : 1.0f;
        float targetPitch = currentPitch_ / std::pow(2.0f, octaveShift);
        
        // Check synth mode
        bool synthMode = synthModeParam_ ? synthModeParam_->load() > 0.5f : true;
        
        // Configure bass synthesizer
        bassSynthesizer_->setFrequency(targetPitch);
        bassSynthesizer_->setAmplitude(currentPitch_ > 0.0f ? 0.3f : 0.0f);
        bassSynthesizer_->setSynthMode(synthMode);
        
        // Generate bass output
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            auto* outputData = buffer.getWritePointer(channel);
            bassSynthesizer_->renderBlock(outputData, numSamples);
        }
        
        // Calculate output level (RMS) from first output channel
        if (totalNumOutputChannels > 0)
        {
            auto* outputData = buffer.getReadPointer(0);
            float outputRMS = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                outputRMS += outputData[i] * outputData[i];
            }
            outputRMS = std::sqrt(outputRMS / numSamples);
            outputLevel_.store(outputRMS);
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
