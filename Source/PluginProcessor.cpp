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

// Debug logging function
static void debugLog(const juce::String& message)
{
    juce::Logger::writeToLog("GuitarToBass: " + message);
}

//==============================================================================
// YIN Algorithm Implementation
YINPitchDetector::YINPitchDetector(int bufferSize, float sampleRate)
    : bufferSize_(bufferSize), sampleRate_(sampleRate)
{
    debugLog("YINPitchDetector initialized - BufferSize: " + juce::String(bufferSize) + 
             ", SampleRate: " + juce::String(sampleRate));
    
    differenceBuffer_.resize(static_cast<size_t>(bufferSize / 2));
    cumulativeBuffer_.resize(static_cast<size_t>(bufferSize / 2));
    
    // Set up overlapping window analysis
    analysisBuffer_.resize(static_cast<size_t>(bufferSize));
    window_.resize(static_cast<size_t>(bufferSize));
    hopSize_ = static_cast<int>(bufferSize * (1.0f - overlapRatio_));
    generateWindow();
    
    // Set up FFT for polyphonic analysis
    fft_ = std::make_unique<juce::dsp::FFT>(fftOrder_);
    int fftSize = 1 << fftOrder_;
    fftBuffer_.resize(static_cast<size_t>(fftSize * 2)); // Complex numbers need 2x space
    spectrum_.resize(static_cast<size_t>(fftSize / 2)); // Only need positive frequencies
    
    debugLog("YINPitchDetector setup complete - HopSize: " + juce::String(hopSize_) + 
             ", FFTSize: " + juce::String(fftSize));
}

float YINPitchDetector::detectPitch(const float* audioBuffer, int numSamples)
{
    // Debug: Check input buffer
    if (audioBuffer == nullptr)
    {
        debugLog("ERROR: detectPitch called with null audio buffer!");
        return lastPitch_;
    }
    
    // Debug: Check for audio activity
    float maxSample = 0.0f;
    float rms = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        maxSample = std::max(maxSample, std::abs(audioBuffer[i]));
        rms += audioBuffer[i] * audioBuffer[i];
    }
    rms = std::sqrt(rms / numSamples);
    
    static int debugCounter = 0;
    if (++debugCounter % 100 == 0) // Log every 100th call
    {
        debugLog("Pitch detection - MaxSample: " + juce::String(maxSample, 6) + 
                 ", RMS: " + juce::String(rms, 6) + ", NumSamples: " + juce::String(numSamples));
    }
    
    // Process input through overlapping window system
    processOverlappingWindow(audioBuffer, numSamples);
    
    // Only run analysis when we have a full buffer
    if (!bufferReady_)
    {
        if (debugCounter % 100 == 0)
            debugLog("Buffer not ready for analysis - WriteIndex: " + juce::String(writeIndex_) + 
                     ", HopSize: " + juce::String(hopSize_) + ", BufferSize: " + juce::String(bufferSize_));
        return lastPitch_;
    }
        
    // Try polyphonic detection first for better chord handling
    if (debugCounter % 100 == 0)
        debugLog("Running pitch detection analysis...");
    
    auto multiplePitches = detectMultiplePitches(analysisBuffer_.data(), bufferSize_);
    float frequency = 0.0f;
    
    if (!multiplePitches.empty())
    {
        // Use lowest detected pitch for bass
        frequency = findLowestPitch(multiplePitches);
        if (debugCounter % 100 == 0)
            debugLog("Polyphonic detection found " + juce::String(multiplePitches.size()) + 
                     " pitches, lowest: " + juce::String(frequency, 1) + " Hz");
    }
    else
    {
        if (debugCounter % 100 == 0)
            debugLog("Polyphonic detection found no pitches, trying YIN...");
        
        // Fall back to YIN for monophonic detection
        std::vector<float> windowedBuffer(static_cast<size_t>(bufferSize_));
        for (int i = 0; i < bufferSize_; ++i)
        {
            windowedBuffer[static_cast<size_t>(i)] = analysisBuffer_[static_cast<size_t>(i)] * window_[static_cast<size_t>(i)];
        }
        
        calculateDifference(windowedBuffer.data(), bufferSize_);
        calculateCumulativeMeanNormalizedDifference();
        
        int periodIndex = getAbsoluteThreshold(0.15f); // Increased threshold for better sensitivity
        if (periodIndex != -1)
        {
            float refinedPeriod = parabolicInterpolation(periodIndex);
            if (refinedPeriod > 0.0f)
            {
                frequency = sampleRate_ / refinedPeriod;
                if (debugCounter % 100 == 0)
                    debugLog("YIN detection - PeriodIndex: " + juce::String(periodIndex) + 
                             ", RefinedPeriod: " + juce::String(refinedPeriod, 3) + 
                             ", Frequency: " + juce::String(frequency, 1) + " Hz");
            }
        }
        else
        {
            if (debugCounter % 100 == 0)
                debugLog("YIN detection - No pitch found (periodIndex = -1)");
        }
    }
    
    // Filter out frequencies outside guitar range
    if (frequency < minFreq_ || frequency > maxFreq_)
    {
        if (debugCounter % 100 == 0 && frequency > 0.0f)
            debugLog("Frequency " + juce::String(frequency, 1) + " Hz outside guitar range (" + 
                     juce::String(minFreq_) + "-" + juce::String(maxFreq_) + " Hz)");
        return lastPitch_;
    }
    
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
        
        if (debugCounter % 100 == 0)
            debugLog("Final pitch: " + juce::String(lastPitch_, 1) + " Hz");
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
        differenceBuffer_[static_cast<size_t>(tau)] = sum;
    }
}

void YINPitchDetector::calculateCumulativeMeanNormalizedDifference()
{
    cumulativeBuffer_[0] = 1.0f;
    float runningSum = 0.0f;
    
    for (int tau = 1; tau < static_cast<int>(differenceBuffer_.size()); ++tau)
    {
        runningSum += differenceBuffer_[static_cast<size_t>(tau)];
        cumulativeBuffer_[static_cast<size_t>(tau)] = differenceBuffer_[static_cast<size_t>(tau)] / (runningSum / tau);
    }
}

int YINPitchDetector::getAbsoluteThreshold(float threshold)
{
    // Start from tau=2 to avoid the trivial minimum at tau=0
    for (int tau = 2; tau < static_cast<int>(cumulativeBuffer_.size()) - 1; ++tau)
    {
        if (cumulativeBuffer_[static_cast<size_t>(tau)] < threshold)
        {
            // Look for local minimum
            while (tau + 1 < static_cast<int>(cumulativeBuffer_.size()) && 
                   cumulativeBuffer_[static_cast<size_t>(tau + 1)] < cumulativeBuffer_[static_cast<size_t>(tau)])
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
        
    float y1 = cumulativeBuffer_[static_cast<size_t>(peakIndex - 1)];
    float y2 = cumulativeBuffer_[static_cast<size_t>(peakIndex)];
    float y3 = cumulativeBuffer_[static_cast<size_t>(peakIndex + 1)];
    
    float x0 = (y3 - y1) / (2.0f * (2.0f * y2 - y1 - y3));
    
    return static_cast<float>(peakIndex) + x0;
}

void YINPitchDetector::processOverlappingWindow(const float* input, int numSamples)
{
    static int windowCounter = 0;
    if (++windowCounter % 1000 == 0) // Log every 1000th call
    {
        debugLog("processOverlappingWindow - WriteIndex: " + juce::String(writeIndex_) + 
                 ", HopSize: " + juce::String(hopSize_) + ", BufferReady: " + juce::String(bufferReady_ ? "true" : "false"));
    }
    
    for (int i = 0; i < numSamples; ++i)
    {
        analysisBuffer_[static_cast<size_t>(writeIndex_)] = input[i];
        writeIndex_++;
        
        // Check if we have a full buffer for analysis
        if (writeIndex_ >= hopSize_)
        {
            bufferReady_ = true;
            if (windowCounter % 1000 == 0)
                debugLog("Buffer now ready for analysis!");
            
            writeIndex_ = 0;
            
            // Shift buffer for overlap
            std::memmove(analysisBuffer_.data(), 
                        analysisBuffer_.data() + hopSize_, 
                        static_cast<size_t>(bufferSize_ - hopSize_) * sizeof(float));
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
        window_[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(static_cast<float>(2.0f * M_PI * x)));
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
        fftBuffer_[static_cast<size_t>(i * 2)] = buffer[i] * window_[static_cast<size_t>(i)]; // Real part
        fftBuffer_[static_cast<size_t>(i * 2 + 1)] = 0.0f; // Imaginary part
    }
    
    // Perform FFT
    fft_->performFrequencyOnlyForwardTransform(fftBuffer_.data());
    
    // Calculate magnitude spectrum
    for (int i = 0; i < fftSize / 2; ++i)
    {
        float real = fftBuffer_[static_cast<size_t>(i * 2)];
        float imag = fftBuffer_[static_cast<size_t>(i * 2 + 1)];
        spectrum_[static_cast<size_t>(i)] = std::sqrt(real * real + imag * imag);
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
        if (spectrum[static_cast<size_t>(i)] > spectrum[static_cast<size_t>(i-1)] && spectrum[static_cast<size_t>(i)] > spectrum[static_cast<size_t>(i+1)] &&
            spectrum[static_cast<size_t>(i)] > spectrum[static_cast<size_t>(i-2)] && spectrum[static_cast<size_t>(i)] > spectrum[static_cast<size_t>(i+2)])
        {
            // Require minimum amplitude threshold
            float maxSpectrum = *std::max_element(spectrum.begin(), spectrum.end());
            if (spectrum[static_cast<size_t>(i)] > maxSpectrum * 0.1f) // 10% of max
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
    debugLog("BassSynthesizer created with sample rate: " + juce::String(sampleRate));
    wavetable_.resize(wavetableSize_);
    generateWavetable();
    debugLog("BassSynthesizer initialization complete");
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
    // Debug: Check output buffer
    if (output == nullptr)
    {
        debugLog("ERROR: BassSynthesizer renderBlock called with null output buffer!");
        return;
    }
    
    // Debug: Log synthesis parameters periodically
    static int synthCounter = 0;
    if (++synthCounter % 10000 == 0) // Log every 10000th call
    {
        debugLog("BassSynthesizer - Frequency: " + juce::String(frequency_, 1) + 
                 ", Amplitude: " + juce::String(amplitude_, 3) + 
                 ", Envelope: " + juce::String(envelope_, 3) + 
                 ", SynthMode: " + juce::String(synthMode_ ? "Wavetable" : "Analog"));
    }
    
    // If amplitude is 0, output silence
    if (amplitude_ <= 0.0f)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            output[i] = 0.0f;
        }
        return;
    }
    
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
            analogPhase_ += static_cast<float>(phaseIncrement_ * 2.0f * M_PI);
            if (analogPhase_ >= 2.0f * M_PI)
                analogPhase_ -= static_cast<float>(2.0f * M_PI);
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
        float angle = static_cast<float>(2.0f * M_PI * x);
        
        // Fundamental + some harmonics for bass character
        float sample = std::sin(angle)                    // Fundamental
                     + 0.3f * std::sin(2.0f * angle)     // 2nd harmonic
                     + 0.15f * std::sin(3.0f * angle)    // 3rd harmonic
                     + 0.1f * std::sin(4.0f * angle);    // 4th harmonic
        
        wavetable_[static_cast<size_t>(i)] = sample * 0.5f; // Scale down
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
        
        return wavetable_[static_cast<size_t>(index1)] * (1.0f - frac) + wavetable_[static_cast<size_t>(index2)] * frac;
    }
    else
    {
        // Analog-style synthesis: sawtooth + lowpass filter
        float sawtooth = static_cast<float>(analogPhase_ / M_PI) - 1.0f; // -1 to 1 sawtooth
        
        // Simple one-pole lowpass filter
        lowPassState_ += (sawtooth - lowPassState_) * filterCutoff_;
        
        return lowPassState_ * 0.5f;
    }
}

//==============================================================================
// Simple real-time pitch detection using zero-crossing analysis
float detectPitchSimple(const float* audioBuffer, int numSamples, double sampleRate)
{
    if (audioBuffer == nullptr || numSamples < 64)
        return 0.0f;
    
    // Count zero crossings
    int zeroCrossings = 0;
    for (int i = 1; i < numSamples; ++i)
    {
        if ((audioBuffer[i-1] < 0.0f && audioBuffer[i] >= 0.0f) ||
            (audioBuffer[i-1] > 0.0f && audioBuffer[i] <= 0.0f))
        {
            zeroCrossings++;
        }
    }
    
    // Calculate frequency from zero crossings
    if (zeroCrossings > 0)
    {
        float frequency = (static_cast<float>(sampleRate) * zeroCrossings) / (2.0f * numSamples);
        
        // Filter to guitar range (80-400 Hz)
        if (frequency >= 80.0f && frequency <= 400.0f)
        {
            return frequency;
        }
    }
    
    return 0.0f;
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
    
    // Input test parameter
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        "inputTest",
        "Input Test",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction(
            [](bool value, int) { return value ? "ON" : "OFF"; })));
    
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
    debugLog("GuitarToBassAudioProcessor constructor called");
    
    // Get parameter pointers for real-time access
    octaveShiftParam_ = parameters_.getRawParameterValue("octaveShift");
    synthModeParam_ = parameters_.getRawParameterValue("synthMode");
    inputTestParam_ = parameters_.getRawParameterValue("inputTest");
    
    debugLog("Parameter pointers obtained - OctaveShift: " + juce::String(octaveShiftParam_ ? "OK" : "NULL") + 
             ", SynthMode: " + juce::String(synthModeParam_ ? "OK" : "NULL"));
    
    // Debug: Check audio bus configuration
    debugLog("Audio Bus Configuration:");
    debugLog("  Input Buses: " + juce::String(getBusCount(true)));
    debugLog("  Output Buses: " + juce::String(getBusCount(false)));
    debugLog("  Main Input Channels: " + juce::String(getMainBusNumInputChannels()));
    debugLog("  Main Output Channels: " + juce::String(getMainBusNumOutputChannels()));
    debugLog("  Total Input Channels: " + juce::String(getTotalNumInputChannels()));
    debugLog("  Total Output Channels: " + juce::String(getTotalNumOutputChannels()));
    
    debugLog("GuitarToBassAudioProcessor initialization complete");
    debugLog("=== PLUGIN LOADED - LOOKING FOR AUDIO ENGINE START ===");
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

void GuitarToBassAudioProcessor::setCurrentProgram ([[maybe_unused]] int index)
{
}

const juce::String GuitarToBassAudioProcessor::getProgramName ([[maybe_unused]] int index)
{
    return {};
}

void GuitarToBassAudioProcessor::changeProgramName ([[maybe_unused]] int index, [[maybe_unused]] const juce::String& newName)
{
}

//==============================================================================
void GuitarToBassAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    debugLog("=== AUDIO ENGINE INITIALIZATION ===");
    debugLog("prepareToPlay called - SampleRate: " + juce::String(sampleRate) + 
             ", SamplesPerBlock: " + juce::String(samplesPerBlock));
    
    // Store sample rate and block size for later use
    currentSampleRate_ = sampleRate;
    currentBlockSize_ = samplesPerBlock;
    
    // Initialize pitch detector with optimized settings for real-time processing
    int pitchAnalysisSize = std::min(512, samplesPerBlock * 2); // Smaller buffer for real-time
    debugLog("Initializing YIN pitch detector with analysis size: " + juce::String(pitchAnalysisSize));
    
    pitchDetector_ = std::make_unique<YINPitchDetector>(pitchAnalysisSize, sampleRate);
    
    // Initialize bass synthesizer
    debugLog("Initializing bass synthesizer");
    bassSynthesizer_ = std::make_unique<BassSynthesizer>(sampleRate);
    
    // Initialize audio buffers
    inputBuffer_.setSize(1, samplesPerBlock);
    outputBuffer_.setSize(1, samplesPerBlock);
    
    debugLog("Audio engine initialization complete");
    debugLog("=== READY FOR AUDIO PROCESSING ===");
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

void GuitarToBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, [[maybe_unused]] juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Debug: Log every process block to confirm it's running
    static int processCounter = 0;
    static bool firstCall = true;
    
    if (firstCall)
    {
        debugLog("=== FIRST PROCESS BLOCK CALLED - AUDIO ENGINE IS RUNNING! ===");
        debugLog("Sample Rate: " + juce::String(getSampleRate()) + " Hz");
        debugLog("Block Size: " + juce::String(getBlockSize()) + " samples");
        debugLog("Input Channels: " + juce::String(totalNumInputChannels));
        debugLog("Output Channels: " + juce::String(totalNumOutputChannels));
        firstCall = false;
    }
    
    if (++processCounter % 50 == 0) // Log every 50th process block (more frequent)
    {
        debugLog("=== PROCESS BLOCK #" + juce::String(processCounter) + " ===");
        debugLog("InputChannels: " + juce::String(totalNumInputChannels) + 
                 ", OutputChannels: " + juce::String(totalNumOutputChannels) + 
                 ", NumSamples: " + juce::String(buffer.getNumSamples()) + 
                 ", SampleRate: " + juce::String(getSampleRate()));
    }

    // Clear any unused output channels
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Generate test tone if input test is enabled
    bool inputTestEnabled = inputTestParam_ ? inputTestParam_->load() > 0.5f : false;
    if (inputTestEnabled)
    {
        debugLog("=== INPUT TEST ENABLED - GENERATING 330Hz TONE ===");
        debugLog("Audio devices selected - checking if audio engine is running...");
        debugLog("Sample Rate: " + juce::String(getSampleRate()) + " Hz");
        debugLog("Block Size: " + juce::String(getBlockSize()) + " samples");
        debugLog("Input Channels: " + juce::String(totalNumInputChannels));
        debugLog("Output Channels: " + juce::String(totalNumOutputChannels));
        static float testPhase = 0.0f;
        float testFreq = 330.0f; // E4 note (within guitar range 80-400 Hz)
        float testIncrement = testFreq / getSampleRate();
        
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            auto* inputData = buffer.getWritePointer(channel);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                inputData[i] = 0.1f * std::sin(testPhase);
                testPhase += static_cast<float>(2.0f * M_PI * testIncrement);
                if (testPhase >= 2.0f * M_PI)
                    testPhase -= static_cast<float>(2.0f * M_PI);
            }
        }
    }
    
    // Perform pitch detection and bass synthesis
    if (totalNumInputChannels > 0 && pitchDetector_ && bassSynthesizer_)
    {
        auto* inputData = buffer.getReadPointer(0);
        int numSamples = buffer.getNumSamples();
        
        // Debug: Check input data
        if (inputData == nullptr)
        {
            debugLog("ERROR: Input data pointer is null!");
            return;
        }
        
        // Check if input data has any non-zero values (for debugging)
        bool hasNonZeroInput = false;
        float maxInputValue = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            if (std::abs(inputData[i]) > 1e-10f)
            {
                hasNonZeroInput = true;
                maxInputValue = std::max(maxInputValue, std::abs(inputData[i]));
            }
        }
        
        if (processCounter % 100 == 0) // Log every 100th process block
        {
            debugLog("=== INPUT DATA CHECK ===");
            debugLog("Has Non-Zero Input: " + juce::String(hasNonZeroInput ? "YES" : "NO"));
            debugLog("Max Input Value: " + juce::String(maxInputValue, 8));
            debugLog("Input Test Enabled: " + juce::String(inputTestEnabled ? "YES" : "NO"));
            debugLog("Processing Live Input: " + juce::String(!inputTestEnabled ? "YES" : "NO"));
        }
        
        // Calculate input level (RMS) from raw input for visualization
        float rawInputRMS = 0.0f;
        float maxRawInputSample = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            rawInputRMS += inputData[i] * inputData[i];
            maxRawInputSample = std::max(maxRawInputSample, std::abs(inputData[i]));
        }
        rawInputRMS = std::sqrt(rawInputRMS / numSamples);
        inputLevel_.store(rawInputRMS); // Store raw input level for visualization
        
        // Apply input gain to boost quiet signals for pitch detection
        const float inputGain = 10.0f; // Boost by 20dB (10x)
        std::vector<float> boostedInput(static_cast<size_t>(numSamples));
        for (int i = 0; i < numSamples; ++i)
        {
            boostedInput[static_cast<size_t>(i)] = inputData[i] * inputGain;
        }
        
        // Calculate boosted input level for pitch detection
        float boostedInputRMS = 0.0f;
        float maxBoostedInputSample = 0.0f;
        for (int i = 0; i < numSamples; ++i)
        {
            boostedInputRMS += boostedInput[static_cast<size_t>(i)] * boostedInput[static_cast<size_t>(i)];
            maxBoostedInputSample = std::max(maxBoostedInputSample, std::abs(boostedInput[static_cast<size_t>(i)]));
        }
        boostedInputRMS = std::sqrt(boostedInputRMS / numSamples);
        
        // Debug: Log input levels periodically
        if (processCounter % 100 == 0) // More frequent logging
        {
            float rawInputDB = 20.0f * std::log10(rawInputRMS + 1e-10f);
            float boostedInputDB = 20.0f * std::log10(boostedInputRMS + 1e-10f);
            debugLog("=== INPUT LEVEL DEBUG ===");
            debugLog("Raw Input RMS: " + juce::String(rawInputRMS, 6));
            debugLog("Raw Input MaxSample: " + juce::String(maxRawInputSample, 6));
            debugLog("Raw Input dB: " + juce::String(rawInputDB, 1) + " dB");
            debugLog("Boosted Input RMS: " + juce::String(boostedInputRMS, 6));
            debugLog("Boosted Input dB: " + juce::String(boostedInputDB, 1) + " dB");
            debugLog("Has Audio (Raw): " + juce::String(rawInputRMS > 0.0001f ? "YES" : "NO"));
            debugLog("Input Gain Applied: 20dB (10x)");
            debugLog("Processing Mode: " + juce::String(inputTestEnabled ? "TEST INPUT" : "LIVE INPUT"));
        }
        
        // Detect pitch using YIN algorithm with real-time fallback
        if (processCounter % 100 == 0) // More frequent logging
        {
            debugLog("=== PITCH DETECTION CALL ===");
            debugLog("Calling pitch detection with input RMS: " + juce::String(boostedInputRMS, 6));
            debugLog("Boosted input samples: " + juce::String(numSamples));
            debugLog("Input Source: " + juce::String(inputTestEnabled ? "Test Tone (330Hz)" : "Live Audio"));
        }
        
        float detectedPitch = 0.0f;
        
        // Try YIN detection first - process both test input and live input the same way
        if (boostedInputRMS > 0.001f) // Only if we have significant audio
        {
            detectedPitch = pitchDetector_->detectPitch(boostedInput.data(), numSamples);
        }
        
        // Real-time fallback: simple zero-crossing detection for immediate response
        if (detectedPitch <= 0.0f && boostedInputRMS > 0.01f) // If YIN fails but we have good audio
        {
            detectedPitch = detectPitchSimple(boostedInput.data(), numSamples, getSampleRate());
            if (processCounter % 100 == 0)
                debugLog("Using simple pitch detection fallback: " + juce::String(detectedPitch, 1) + " Hz");
        }
        
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
        
        // Debug: Log synthesis parameters periodically
        if (processCounter % 100 == 0) // More frequent logging
        {
            debugLog("=== SYNTHESIS DEBUG ===");
            debugLog("Detected Pitch: " + juce::String(detectedPitch, 1) + " Hz");
            debugLog("Current Pitch: " + juce::String(currentPitch_, 1) + " Hz");
            debugLog("Target Pitch: " + juce::String(targetPitch, 1) + " Hz");
            debugLog("Octave Shift: " + juce::String(octaveShift, 1) + " octaves");
            debugLog("Synth Mode: " + juce::String(synthMode ? "Synth" : "Analog"));
            debugLog("Input Source: " + juce::String(inputTestEnabled ? "Test Tone" : "Live Audio"));
        }
        
        // Configure bass synthesizer
        bassSynthesizer_->setFrequency(targetPitch);
        
        // Only produce output if we have detected pitch and input audio
        bool hasInputAudio = boostedInputRMS > 0.001f;
        bool hasValidPitch = currentPitch_ > 0.0f;
        
        if (hasInputAudio && hasValidPitch)
        {
            bassSynthesizer_->setAmplitude(0.3f);
        }
        else
        {
            // No output when no input or no pitch detected
            bassSynthesizer_->setAmplitude(0.0f);
            
            // Reset synthesizer when no input to ensure clean state
            if (!hasInputAudio)
            {
                bassSynthesizer_->reset();
            }
        }
        
        bassSynthesizer_->setSynthMode(synthMode);
        
        // Generate bass output
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            auto* outputData = buffer.getWritePointer(channel);
            if (outputData == nullptr)
            {
                debugLog("ERROR: Output data pointer is null for channel " + juce::String(channel));
                continue;
            }
            bassSynthesizer_->renderBlock(outputData, numSamples);
        }
        
        // Calculate output level (RMS) from first output channel
        if (totalNumOutputChannels > 0)
        {
            auto* outputData = buffer.getReadPointer(0);
            if (outputData != nullptr)
            {
                float outputRMS = 0.0f;
                float maxOutputSample = 0.0f;
                for (int i = 0; i < numSamples; ++i)
                {
                    outputRMS += outputData[i] * outputData[i];
                    maxOutputSample = std::max(maxOutputSample, std::abs(outputData[i]));
                }
                outputRMS = std::sqrt(outputRMS / numSamples);
                outputLevel_.store(outputRMS);
                
                // Debug: Log output levels periodically
                if (processCounter % 100 == 0) // More frequent logging
                {
                    float outputDB = 20.0f * std::log10(outputRMS + 1e-10f);
                    debugLog("=== OUTPUT LEVEL DEBUG ===");
                    debugLog("Output RMS: " + juce::String(outputRMS, 6));
                    debugLog("Output MaxSample: " + juce::String(maxOutputSample, 6));
                    debugLog("Output dB: " + juce::String(outputDB, 1) + " dB");
                    debugLog("Input Source: " + juce::String(inputTestEnabled ? "Test Tone" : "Live Audio"));
                }
            }
        }
    }
    else
    {
        // Debug: Log when processing is skipped
        if (processCounter % 1000 == 0)
        {
            debugLog("Processing skipped - InputChannels: " + juce::String(totalNumInputChannels) + 
                     ", PitchDetector: " + juce::String(pitchDetector_ ? "OK" : "NULL") + 
                     ", BassSynthesizer: " + juce::String(bassSynthesizer_ ? "OK" : "NULL"));
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
