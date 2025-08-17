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
#include <string>

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
    float confidence = 0.0f;
    
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
        
        int periodIndex = getAbsoluteThreshold(0.08f); // Lowered threshold for better sensitivity
        if (periodIndex != -1)
        {
            float refinedPeriod = parabolicInterpolation(periodIndex);
            if (refinedPeriod > 0.0f)
            {
                frequency = sampleRate_ / refinedPeriod;
                confidence = calculatePitchConfidence(periodIndex);
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
    
    // Enhanced pitch validation with confidence
    
    // Filter out frequencies outside guitar range
    if (frequency < minFreq_ || frequency > maxFreq_)
    {
        if (debugCounter % 100 == 0 && frequency > 0.0f)
            debugLog("Frequency " + juce::String(frequency, 1) + " Hz outside guitar range (" + 
                     juce::String(minFreq_) + "-" + juce::String(maxFreq_) + " Hz)");
        consecutiveDetections_ = 0;
        return lastPitch_;
    }
    
    // Validate pitch candidate with confidence
    if (!validatePitchCandidate(frequency, confidence))
    {
        consecutiveDetections_ = 0;
        return lastPitch_;
    }
    
    // Apply enhanced smoothing with consecutive detection validation
    if (frequency > 0.0f && confidence > confidenceThreshold_)
    {
        consecutiveDetections_++;
        
        // Only update pitch if we have enough consecutive detections
        if (consecutiveDetections_ >= minConsecutiveDetections_)
        {
            if (lastPitch_ > 0.0f)
            {
                // Adaptive smoothing based on pitch stability
                float pitchDifference = std::abs(frequency - lastPitch_) / lastPitch_;
                float adaptiveSmoothing = pitchDifference > 0.1f ? 0.3f : pitchSmoothing_;
                lastPitch_ = lastPitch_ * adaptiveSmoothing + frequency * (1.0f - adaptiveSmoothing);
            }
            else
            {
                lastPitch_ = frequency;
            }
            
            pitchConfidence_ = confidence;
        }
        
        if (debugCounter % 100 == 0)
            debugLog("Pitch: " + juce::String(frequency, 1) + " Hz, Confidence: " + 
                    juce::String(confidence, 3) + ", Consecutive: " + juce::String(consecutiveDetections_) + 
                    ", Final: " + juce::String(lastPitch_, 1) + " Hz");
    }
    else
    {
        consecutiveDetections_ = 0;
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
    std::vector<std::pair<float, float>> peaks; // frequency, amplitude
    
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
            // Enhanced amplitude threshold with dynamic adjustment
            float maxSpectrum = *std::max_element(spectrum.begin(), spectrum.end());
            float dynamicThreshold = std::max(0.1f, maxSpectrum * 0.12f); // Adaptive threshold
            if (spectrum[static_cast<size_t>(i)] > dynamicThreshold)
            {
                peaks.push_back({frequency, spectrum[static_cast<size_t>(i)]});
            }
        }
    }
    
    // Sort peaks by amplitude (strongest first)
    std::sort(peaks.begin(), peaks.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });
    
    // Filter out harmonics - only keep fundamentals
    for (const auto& peak : peaks)
    {
        float frequency = peak.first;
        bool isHarmonic = false;
        
        // Check if this frequency is a harmonic of an already detected fundamental
        for (float fundamental : pitches)
        {
            // Check if frequency is close to 2x, 3x, 4x the fundamental
            for (int harmonic = 2; harmonic <= 4; ++harmonic)
            {
                float harmonicFreq = fundamental * harmonic;
                if (std::abs(frequency - harmonicFreq) < 10.0f) // Within 10Hz tolerance
                {
                    isHarmonic = true;
                    break;
                }
            }
            if (isHarmonic) break;
        }
        
        if (!isHarmonic)
        {
            pitches.push_back(frequency);
        }
        
        // Limit to 3 simultaneous notes maximum for guitar chords
        if (pitches.size() >= 3) break;
    }
    
    return pitches;
}

float YINPitchDetector::findLowestPitch(const std::vector<float>& pitches)
{
    if (pitches.empty())
        return 0.0f;
        
    return *std::min_element(pitches.begin(), pitches.end());
}

float YINPitchDetector::calculatePitchConfidence(int periodIndex)
{
    if (periodIndex <= 0 || periodIndex >= static_cast<int>(cumulativeBuffer_.size()))
        return 0.0f;
    
    // Confidence is inversely related to the YIN value at the detected period
    float yinValue = cumulativeBuffer_[static_cast<size_t>(periodIndex)];
    
    // Lower YIN values indicate better periodicity (higher confidence)
    float confidence = 1.0f - std::min(yinValue, 1.0f);
    
    // Boost confidence for very low YIN values
    if (yinValue < 0.1f)
        confidence = 0.9f + (0.1f - yinValue) * 10.0f; // Scale from 0.9 to 1.0
    
    return juce::jlimit(0.0f, 1.0f, confidence);
}

std::vector<float> YINPitchDetector::preprocessAudio(const float* input, int numSamples)
{
    std::vector<float> processed(static_cast<size_t>(numSamples));
    
    // Apply pre-emphasis filter to enhance higher frequencies
    float preEmphasisCoeff = 0.97f;
    float prevSample = 0.0f;
    
    for (int i = 0; i < numSamples; ++i)
    {
        processed[static_cast<size_t>(i)] = input[i] - preEmphasisCoeff * prevSample;
        prevSample = input[i];
    }
    
    // Apply simple DC removal
    float dcSum = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        dcSum += processed[static_cast<size_t>(i)];
    }
    float dcOffset = dcSum / numSamples;
    
    for (int i = 0; i < numSamples; ++i)
    {
        processed[static_cast<size_t>(i)] -= dcOffset;
    }
    
    return processed;
}

float YINPitchDetector::detectPitchWithConfidence(const float* audioBuffer, int numSamples, float& confidence)
{
    // Preprocess audio for better pitch detection
    auto processedAudio = preprocessAudio(audioBuffer, numSamples);
    
    // Apply window
    for (int i = 0; i < numSamples && i < bufferSize_; ++i)
    {
        processedAudio[static_cast<size_t>(i)] *= window_[static_cast<size_t>(i)];
    }
    
    // Run YIN algorithm
    calculateDifference(processedAudio.data(), std::min(numSamples, bufferSize_));
    calculateCumulativeMeanNormalizedDifference();
    
    int periodIndex = getAbsoluteThreshold(0.08f);
    if (periodIndex == -1)
    {
        confidence = 0.0f;
        return 0.0f;
    }
    
    float refinedPeriod = parabolicInterpolation(periodIndex);
    if (refinedPeriod <= 0.0f)
    {
        confidence = 0.0f;
        return 0.0f;
    }
    
    float frequency = sampleRate_ / refinedPeriod;
    confidence = calculatePitchConfidence(periodIndex);
    
    return frequency;
}

bool YINPitchDetector::validatePitchCandidate(float frequency, float confidence)
{
    // Check frequency range
    if (frequency < minFreq_ || frequency > maxFreq_)
        return false;
    
    // Check minimum confidence
    if (confidence < confidenceThreshold_)
        return false;
    
    // Check for reasonable pitch change if we have a previous pitch
    if (lastPitch_ > 0.0f)
    {
        float pitchRatio = frequency / lastPitch_;
        
        // Reject pitches that are too far from the previous pitch (octave jumps are suspicious)
        if (pitchRatio > 2.1f || pitchRatio < 0.48f) // Allow just over an octave
        {
            return false;
        }
        
        // Be more strict about smaller changes to reduce jitter
        float centDifference = 1200.0f * std::log2(pitchRatio);
        if (std::abs(centDifference) > 100.0f && confidence < 0.8f) // 100 cents = 1 semitone
        {
            return false;
        }
    }
    
    return true;
}

//==============================================================================
// Multi-Instrument Synthesizer Implementation
MultiInstrumentSynthesizer::MultiInstrumentSynthesizer(float sampleRate)
    : sampleRate_(sampleRate)
{
    debugLog("MultiInstrumentSynthesizer created with sample rate: " + juce::String(sampleRate));
    wavetable_.resize(wavetableSize_);
    generateWavetable();
    debugLog("MultiInstrumentSynthesizer initialization complete");
}

void MultiInstrumentSynthesizer::setFrequency(float frequency)
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

void MultiInstrumentSynthesizer::setAmplitude(float amplitude)
{
    amplitude_ = juce::jlimit(0.0f, 1.0f, amplitude);
}

void MultiInstrumentSynthesizer::setInstrumentMode(InstrumentMode mode)
{
    if (instrumentMode_ != mode)
    {
        instrumentMode_ = mode;
        switch (instrumentMode_)
        {
            case InstrumentMode::SynthBass:
                generateWavetable();
                break;
            case InstrumentMode::Piano:
                generatePianoWavetable();
                break;
            case InstrumentMode::AnalogBass:
            default:
                break;
        }
    }
}

void MultiInstrumentSynthesizer::renderBlock(float* output, int numSamples)
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
        debugLog("MultiInstrumentSynthesizer - Frequency: " + juce::String(frequency_, 1) + 
                 ", Amplitude: " + juce::String(amplitude_, 3) + 
                 ", Envelope: " + juce::String(envelope_, 3) + 
                 ", Mode: " + juce::String(static_cast<int>(instrumentMode_)));
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
        if (instrumentMode_ == InstrumentMode::SynthBass || instrumentMode_ == InstrumentMode::Piano)
        {
            phase_ += phaseIncrement_;
            if (phase_ >= 1.0f)
                phase_ -= 1.0f;
        }
        else // AnalogBass
        {
            analogPhase_ += static_cast<float>(phaseIncrement_ * 2.0f * M_PI);
            if (analogPhase_ >= 2.0f * M_PI)
                analogPhase_ -= static_cast<float>(2.0f * M_PI);
        }
    }
}

void MultiInstrumentSynthesizer::reset()
{
    phase_ = 0.0f;
    analogPhase_ = 0.0f;
    envelope_ = 0.0f;
    lowPassState_ = 0.0f;
}

void MultiInstrumentSynthesizer::generateWavetable()
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

void MultiInstrumentSynthesizer::generatePianoWavetable()
{
    // Generate a piano-like wavetable with more complex harmonics
    for (int i = 0; i < wavetableSize_; ++i)
    {
        float x = static_cast<float>(i) / wavetableSize_;
        float angle = static_cast<float>(2.0f * M_PI * x);
        
        // Piano-like harmonic series with overtones
        float sample = 0.8f * std::sin(angle)                      // Fundamental
                     + 0.4f * std::sin(2.0f * angle)               // 2nd harmonic
                     + 0.2f * std::sin(3.0f * angle)               // 3rd harmonic
                     + 0.15f * std::sin(4.0f * angle)              // 4th harmonic
                     + 0.1f * std::sin(5.0f * angle)               // 5th harmonic
                     + 0.05f * std::sin(6.0f * angle)              // 6th harmonic
                     + 0.03f * std::sin(8.0f * angle);             // 8th harmonic
        
        wavetable_[static_cast<size_t>(i)] = sample * 0.4f; // Scale down for piano
    }
}

void MultiInstrumentSynthesizer::generateAnalogWave()
{
    // This method can be used for setting up analog-style parameters
    // For now, analog synthesis is handled directly in getNextSample()
    filterCutoff_ = 0.3f; // Reset filter cutoff for analog bass
}

float MultiInstrumentSynthesizer::getNextSample()
{
    switch (instrumentMode_)
    {
        case InstrumentMode::SynthBass:
        case InstrumentMode::Piano:
        {
            // Wavetable synthesis with linear interpolation
            float floatIndex = phase_ * wavetableSize_;
            int index1 = static_cast<int>(floatIndex);
            int index2 = (index1 + 1) % wavetableSize_;
            float frac = floatIndex - index1;
            
            return wavetable_[static_cast<size_t>(index1)] * (1.0f - frac) + wavetable_[static_cast<size_t>(index2)] * frac;
        }
        case InstrumentMode::AnalogBass:
        default:
        {
            // Analog-style synthesis: sawtooth + lowpass filter
            float sawtooth = static_cast<float>(analogPhase_ / M_PI) - 1.0f; // -1 to 1 sawtooth
            
            // Simple one-pole lowpass filter
            lowPassState_ += (sawtooth - lowPassState_) * filterCutoff_;
            
            return lowPassState_ * 0.5f;
        }
    }
}

//==============================================================================
// Note Detection Implementation
int NoteDetector::frequencyToMidiNote(float frequency)
{
    if (frequency <= 0.0f) return -1;
    
    // MIDI note formula: midiNote = 69 + 12 * log2(frequency / 440)
    // where A4 (440 Hz) = MIDI note 69
    float midiNote = 69.0f + 12.0f * std::log2(frequency / 440.0f);
    return static_cast<int>(std::round(midiNote));
}

float NoteDetector::midiNoteToFrequency(int midiNote)
{
    if (midiNote < 0) return 0.0f;
    
    // Frequency formula: frequency = 440 * 2^((midiNote - 69) / 12)
    return 440.0f * std::pow(2.0f, (midiNote - 69) / 12.0f);
}

std::string NoteDetector::midiNoteToNoteName(int midiNote)
{
    if (midiNote < 0) return "---";
    
    const std::vector<std::string> noteNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    
    int octave = (midiNote / 12) - 1;
    int noteIndex = midiNote % 12;
    
    return noteNames[static_cast<size_t>(noteIndex)] + std::to_string(octave);
}

int NoteDetector::noteNameToMidiNote(const std::string& noteName)
{
    if (noteName.length() < 2) return -1;
    
    const std::vector<std::string> noteNames = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    
    // Extract note name (first 1-2 characters)
    std::string baseName = noteName.substr(0, noteName.find_first_of("0123456789"));
    std::string octaveStr = noteName.substr(baseName.length());
    
    // Find note index
    auto it = std::find(noteNames.begin(), noteNames.end(), baseName);
    if (it == noteNames.end()) return -1;
    
    int noteIndex = static_cast<int>(std::distance(noteNames.begin(), it));
    int octave = std::stoi(octaveStr);
    
    return (octave + 1) * 12 + noteIndex;
}

//==============================================================================
// Chord Root Detector Implementation
ChordRootDetector::ChordRootDetector(float sampleRate, float stabilityDelayMs)
    : sampleRate_(sampleRate)
{
    // Calculate stability delay in samples based on typical buffer sizes
    stabilityDelaySamples_ = static_cast<int>((stabilityDelayMs / 1000.0f) * sampleRate);
    stabilityBuffer_.resize(stabilityBufferSize_);
    reset();
}

ChordRootDetector::ChordInfo ChordRootDetector::analyzeNotes(const std::vector<float>& frequencies)
{
    currentSampleCount_++;
    
    // Convert frequencies to notes
    std::vector<NoteDetector::Note> detectedNotes;
    for (float freq : frequencies)
    {
        if (freq > 0.0f)
        {
            detectedNotes.emplace_back(freq, 1.0f); // Assume confidence 1.0 for now
        }
    }
    
    // Create new chord info
    ChordInfo newChord;
    newChord.detectedNotes = detectedNotes;
    
    if (!detectedNotes.empty())
    {
        newChord.rootNote = findChordRoot(detectedNotes);
        newChord.confidence = 1.0f / detectedNotes.size(); // Simple confidence metric
    }
    
    // Update stability buffer
    updateStabilityBuffer(newChord);
    
    // Check if chord is stable (has been consistent for stability delay period)
    if (isChordStable(newChord))
    {
        newChord.isStable = true;
        currentChord_ = newChord;
    }
    
    return currentChord_;
}

NoteDetector::Note ChordRootDetector::findChordRoot(const std::vector<NoteDetector::Note>& notes)
{
    if (notes.empty()) return NoteDetector::Note();
    
    // Simple root detection: find the lowest note
    // In future versions, this could be enhanced with chord theory analysis
    auto lowestNote = *std::min_element(notes.begin(), notes.end(), 
        [](const NoteDetector::Note& a, const NoteDetector::Note& b) {
            return a.frequency < b.frequency;
        });
    
    return lowestNote;
}

bool ChordRootDetector::isChordStable(const ChordInfo& newChord)
{
    if (!bufferFull_) return false;
    
    // Check if the chord has been consistent across the stability buffer
    int consistentCount = 0;
    for (const auto& bufferedChord : stabilityBuffer_)
    {
        if (bufferedChord.isValid() && newChord.isValid())
        {
            // Consider chords similar if root notes are within threshold
            float semitoneDiff = std::abs(newChord.rootNote.midiNote - bufferedChord.rootNote.midiNote);
            if (semitoneDiff <= chordChangeThreshold_)
            {
                consistentCount++;
            }
        }
        else if (!bufferedChord.isValid() && !newChord.isValid())
        {
            consistentCount++; // Both are silent/invalid
        }
    }
    
    // Require 80% consistency for stability
    return consistentCount >= (stabilityBufferSize_ * 0.8f);
}

void ChordRootDetector::updateStabilityBuffer(const ChordInfo& chord)
{
    stabilityBuffer_[static_cast<size_t>(bufferWriteIndex_)] = chord;
    bufferWriteIndex_ = (bufferWriteIndex_ + 1) % stabilityBufferSize_;
    
    if (bufferWriteIndex_ == 0)
    {
        bufferFull_ = true;
    }
}

void ChordRootDetector::reset()
{
    currentChord_ = ChordInfo();
    pendingChord_ = ChordInfo();
    currentSampleCount_ = 0;
    bufferWriteIndex_ = 0;
    bufferFull_ = false;
    
    for (auto& chord : stabilityBuffer_)
    {
        chord = ChordInfo();
    }
}

//==============================================================================
// Bass Note Mapper Implementation
BassNoteMapper::BassNoteMapper()
{
    initializeBassTuning();
}

void BassNoteMapper::initializeBassTuning()
{
    // Standard 4-string bass tuning: E1, A1, D2, G2
    bassTuning_.clear();
    
    BassNote e1;
    e1.midiNote = NoteDetector::noteNameToMidiNote("E1");
    e1.frequency = NoteDetector::midiNoteToFrequency(e1.midiNote);
    e1.noteName = "E1";
    e1.stringName = "E (4th string)";
    bassTuning_.push_back(e1);
    
    BassNote a1;
    a1.midiNote = NoteDetector::noteNameToMidiNote("A1");
    a1.frequency = NoteDetector::midiNoteToFrequency(a1.midiNote);
    a1.noteName = "A1";
    a1.stringName = "A (3rd string)";
    bassTuning_.push_back(a1);
    
    BassNote d2;
    d2.midiNote = NoteDetector::noteNameToMidiNote("D2");
    d2.frequency = NoteDetector::midiNoteToFrequency(d2.midiNote);
    d2.noteName = "D2";
    d2.stringName = "D (2nd string)";
    bassTuning_.push_back(d2);
    
    BassNote g2;
    g2.midiNote = NoteDetector::noteNameToMidiNote("G2");
    g2.frequency = NoteDetector::midiNoteToFrequency(g2.midiNote);
    g2.noteName = "G2";
    g2.stringName = "G (1st string)";
    bassTuning_.push_back(g2);
}

BassNoteMapper::BassNote BassNoteMapper::mapToClosestBassNote(int inputMidiNote)
{
    if (inputMidiNote < 0 || bassTuning_.empty())
    {
        return BassNote(); // Invalid note
    }
    
    int closestIndex = findClosestBassNoteIndex(inputMidiNote);
    return bassTuning_[static_cast<size_t>(closestIndex)];
}

BassNoteMapper::BassNote BassNoteMapper::mapToClosestBassNote(float inputFrequency)
{
    int midiNote = NoteDetector::frequencyToMidiNote(inputFrequency);
    return mapToClosestBassNote(midiNote);
}

BassNoteMapper::BassNote BassNoteMapper::mapChordRootToBass(const NoteDetector::Note& chordRoot)
{
    if (chordRoot.midiNote < 0)
    {
        return BassNote(); // Invalid chord root
    }
    
    // Transpose the chord root down to bass range by finding the note name 
    // and mapping it to the appropriate bass octave
    int noteClass = chordRoot.midiNote % 12; // Get the note class (C=0, C#=1, D=2, etc.)
    
    // Map note classes to preferred bass notes
    // We want to map to the bass strings, prioritizing lower strings for lower notes
    int targetBassNote = -1;
    
    switch (noteClass) {
        case 0:  // C -> closest is D2 (2 semitones up) or A1 (3 semitones down)
        case 1:  // C# -> closest is D2 (1 semitone up) 
        case 2:  // D -> D2 directly
            targetBassNote = 38; // D2
            break;
        case 3:  // D# -> closest is E1 (4 semitones down) or D2 (1 semitone down)  
        case 4:  // E -> E1 directly (but transpose down octaves)
            targetBassNote = 28; // E1 
            break;
        case 5:  // F -> closest is E1 (1 semitone down)
        case 6:  // F# -> closest is G2 (1 semitone up) or E1 (2 semitones down)
        case 7:  // G -> G2 directly
            targetBassNote = 43; // G2
            break;
        case 8:  // G# -> closest is A1 (1 semitone up) or G2 (1 semitone down)
        case 9:  // A -> A1 directly  
            targetBassNote = 33; // A1
            break;
        case 10: // A# -> closest is A1 (1 semitone down)
        case 11: // B -> closest is A1 (2 semitones down) 
            targetBassNote = 33; // A1
            break;
    }
    
    // Find the bass note that matches our target
    for (const auto& bassNote : bassTuning_)
    {
        if (bassNote.midiNote == targetBassNote)
        {
            return bassNote;
        }
    }
    
    // Fallback to closest bass note if something went wrong
    return mapToClosestBassNote(chordRoot.midiNote);
}

int BassNoteMapper::findClosestBassNoteIndex(int midiNote)
{
    if (bassTuning_.empty()) return 0;
    
    int closestIndex = 0;
    int minDistance = std::abs(midiNote - bassTuning_[0].midiNote);
    
    for (size_t i = 1; i < bassTuning_.size(); ++i)
    {
        int distance = std::abs(midiNote - bassTuning_[i].midiNote);
        if (distance < minDistance)
        {
            minDistance = distance;
            closestIndex = static_cast<int>(i);
        }
    }
    
    return closestIndex;
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
        juce::ParameterID { "octaveShift", 1 },
        "Octave Shift", 
        juce::NormalisableRange<float>(0.0f, 4.0f, 1.0f), 
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("octaves").withStringFromValueFunction(
            [](float value, int) { return juce::String(static_cast<int>(value)) + " oct"; })));
    
    // Instrument mode parameter (0 = analog bass, 1 = synth bass, 2 = piano)
    parameters.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "instrumentMode", 1 },
        "Instrument",
        juce::StringArray { "Analog Bass", "Synth Bass", "Piano" },
        1)); // Default to Synth Bass    
    // Input test parameter
    parameters.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "inputTest", 1 },
        "Input Test",
        false,
        juce::AudioParameterBoolAttributes().withStringFromValueFunction(
            [](bool value, int) { return value ? "ON" : "OFF"; })));
    
    // Add noise gate threshold parameter
    parameters.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "gateThreshold", 1 },
        "Gate Threshold", 
        juce::NormalisableRange<float>(-80.0f, -20.0f, 1.0f), -50.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB").withStringFromValueFunction(
            [](float value, int) { return juce::String(value, 0) + " dB"; })));
    
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
    instrumentModeParam_ = parameters_.getRawParameterValue("instrumentMode");
    inputTestParam_ = parameters_.getRawParameterValue("inputTest");
    gateThresholdParam_ = parameters_.getRawParameterValue("gateThreshold");
    
    debugLog("Parameter pointers obtained - OctaveShift: " + juce::String(octaveShiftParam_ ? "OK" : "NULL") + 
             ", InstrumentMode: " + juce::String(instrumentModeParam_ ? "OK" : "NULL"));
    
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
    
    // Initialize multi-instrument synthesizer
    debugLog("Initializing multi-instrument synthesizer");
    instrumentSynthesizer_ = std::make_unique<MultiInstrumentSynthesizer>(sampleRate);
    
    // Initialize chord detector with 100ms stability delay
    debugLog("Initializing chord root detector");
    chordDetector_ = std::make_unique<ChordRootDetector>(sampleRate, 100.0f);
    
    // Initialize bass note mapper
    debugLog("Initializing bass note mapper");
    bassMapper_ = std::make_unique<BassNoteMapper>();
    
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

void GuitarToBassAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
        float testIncrement = testFreq / static_cast<float>(getSampleRate());
        
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
    
    // Perform pitch detection and synthesis
    if (totalNumInputChannels > 0 && pitchDetector_ && instrumentSynthesizer_)
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
        float inputGain = 10.0f; // 20dB boost
        float boostedInputRMS = 0.0f;
        float maxBoostedSample = 0.0f;
        
        // Apply noise gate to prevent background noise from causing unwanted output
        float gateThreshold = gateThresholdParam_ ? gateThresholdParam_->load() : -50.0f;
        float gateThresholdLinear = std::pow(10.0f, gateThreshold / 20.0f);
        
        // Calculate gate coefficients for attack and release
        float gateAttackCoeff = gateAttack_ > 0.0f ? static_cast<float>(std::exp(-1.0f / (getSampleRate() * gateAttack_))) : 0.0f;
        float gateReleaseCoeff = gateRelease_ > 0.0f ? static_cast<float>(std::exp(-1.0f / (getSampleRate() * gateRelease_))) : 0.0f;
        
        // Apply noise gate and gain to input signal
        std::vector<float> gatedInput(static_cast<size_t>(numSamples));
        for (int i = 0; i < numSamples; ++i)
        {
            float boostedSample = inputData[i] * inputGain;
            
            // Calculate gate level based on input amplitude
            float inputAmplitude = std::abs(boostedSample);
            float targetGateLevel = inputAmplitude > gateThresholdLinear ? 1.0f : 0.0f;
            
            // Smooth gate level with attack/release
            if (targetGateLevel > gateLevel_)
                gateLevel_ = gateLevel_ + (1.0f - gateAttackCoeff) * (targetGateLevel - gateLevel_);
            else
                gateLevel_ = gateLevel_ + (1.0f - gateReleaseCoeff) * (targetGateLevel - gateLevel_);
            
            // Apply gate to boosted sample
            gatedInput[static_cast<size_t>(i)] = boostedSample * gateLevel_;
            
            boostedInputRMS += gatedInput[static_cast<size_t>(i)] * gatedInput[static_cast<size_t>(i)];
            maxBoostedSample = std::max(maxBoostedSample, std::abs(gatedInput[static_cast<size_t>(i)]));
        }
        boostedInputRMS = std::sqrt(boostedInputRMS / numSamples);
        
        // Debug: Log input levels periodically
        if (processCounter % 100 == 0) // More frequent logging
        {
            float rawInputDB = 20.0f * std::log10(rawInputRMS + 1e-10f);
            float boostedInputDB = 20.0f * std::log10(boostedInputRMS + 1e-10f);
            float gateLevelDB = 20.0f * std::log10(gateLevel_ + 1e-10f);
            debugLog("=== INPUT LEVEL DEBUG ===");
            debugLog("Raw Input RMS: " + juce::String(rawInputRMS, 6));
            debugLog("Raw Input MaxSample: " + juce::String(maxRawInputSample, 6));
            debugLog("Raw Input dB: " + juce::String(rawInputDB, 1) + " dB");
            debugLog("Boosted Input RMS: " + juce::String(boostedInputRMS, 6));
            debugLog("Boosted Input dB: " + juce::String(boostedInputDB, 1) + " dB");
            debugLog("Gate Threshold: " + juce::String(gateThreshold, 1) + " dB");
            debugLog("Gate Level: " + juce::String(gateLevel_, 3) + " (" + juce::String(gateLevelDB, 1) + " dB)");
            debugLog("Has Audio (Raw): " + juce::String(rawInputRMS > 0.001f ? "YES" : "NO"));
            debugLog("Has Audio (Gated): " + juce::String(boostedInputRMS > 0.001f ? "YES" : "NO"));
        }
        
        // Only perform pitch detection if we have significant gated input
        bool hasSignificantInput = boostedInputRMS > 0.001f;
        if (!hasSignificantInput)
        {
            // No significant input - clear pitch and output
            currentPitch_ = 0.0f;
            instrumentSynthesizer_->setAmplitude(0.0f);
            instrumentSynthesizer_->reset();
            
            // Debug: Log when input is too quiet
            if (processCounter % 1000 == 0) // Less frequent for quiet periods
            {
                debugLog("=== NO SIGNIFICANT INPUT - GATE CLOSED ===");
                debugLog("Input too quiet for pitch detection - gate threshold: " + juce::String(gateThreshold, 1) + " dB");
            }
            return;
        }
        
        // Perform polyphonic pitch detection using YIN for multiple notes
        std::vector<float> detectedPitches;
        float primaryPitch = 0.0f;
        
        if (pitchDetector_)
        {
            // Get multiple pitches from polyphonic detection
            detectedPitches = pitchDetector_->detectMultiplePitches(gatedInput.data(), numSamples);
            
            // Also get primary pitch from traditional YIN
            primaryPitch = pitchDetector_->detectPitch(gatedInput.data(), numSamples);
            
            // If polyphonic detection found pitches but YIN didn't, add the lowest polyphonic pitch
            if (detectedPitches.empty() && primaryPitch > 0.0f)
            {
                detectedPitches.push_back(primaryPitch);
            }
        }
        
        // Fallback to simple pitch detection if both methods fail
        if (detectedPitches.empty() && primaryPitch <= 0.0f)
        {
            float simplePitch = detectPitchSimple(gatedInput.data(), numSamples, getSampleRate());
            if (simplePitch > 0.0f)
            {
                detectedPitches.push_back(simplePitch);
                primaryPitch = simplePitch;
            }
        }
        
        // Update note-based detection system
        float targetPitch = 0.0f;
        if (chordDetector_ && bassMapper_ && !detectedPitches.empty())
        {
            // Analyze chord from detected pitches
            currentChord_ = chordDetector_->analyzeNotes(detectedPitches);
            
            // Convert frequencies to notes for visualization
            currentDetectedNotes_.clear();
            for (float pitch : detectedPitches)
            {
                if (pitch > 0.0f)
                {
                    currentDetectedNotes_.emplace_back(pitch, 1.0f);
                }
            }
            
            // Use chord root detection with stability for bass note mapping
            if (currentChord_.isValid() && currentChord_.isStable)
            {
                currentBassNote_ = bassMapper_->mapChordRootToBass(currentChord_.rootNote);
                targetPitch = currentBassNote_.frequency;
                
                if (processCounter % 100 == 0)
                {
                    debugLog("=== NOTE-BASED DETECTION ===");
                    debugLog("Detected Pitches: " + juce::String(detectedPitches.size()));
                    for (size_t i = 0; i < std::min<size_t>(detectedPitches.size(), 5); ++i) // Log up to 5 pitches
                    {
                        int midiNote = NoteDetector::frequencyToMidiNote(detectedPitches[i]);
                        std::string noteName = NoteDetector::midiNoteToNoteName(midiNote);
                        debugLog("  Note " + juce::String(static_cast<int>(i+1)) + ": " + 
                                juce::String(noteName) + " (" + juce::String(detectedPitches[i], 1) + " Hz)");
                    }
                    debugLog("Chord Root: " + juce::String(currentChord_.rootNote.noteName) + 
                             " (" + juce::String(currentChord_.rootNote.frequency, 1) + " Hz) MIDI: " + 
                             juce::String(currentChord_.rootNote.midiNote));
                    debugLog("Note Class: " + juce::String(currentChord_.rootNote.midiNote % 12));
                    debugLog("Mapped Bass Note: " + juce::String(currentBassNote_.noteName) + 
                             " (" + juce::String(currentBassNote_.frequency, 1) + " Hz) MIDI: " + 
                             juce::String(currentBassNote_.midiNote));
                    debugLog("Bass String: " + juce::String(currentBassNote_.stringName));
                }
            }
            else
            {
                // Use primary detected pitch while waiting for chord stability
                targetPitch = primaryPitch > 0.0f ? primaryPitch : 
                             (!detectedPitches.empty() ? detectedPitches[0] : 0.0f);
                
                if (processCounter % 100 == 0)
                {
                    debugLog("Chord not stable yet - using primary pitch: " + 
                            juce::String(targetPitch, 1) + " Hz");
                }
            }
        }
        else
        {
            // Fall back to primary pitch
            targetPitch = primaryPitch > 0.0f ? primaryPitch : 
                         (!detectedPitches.empty() ? detectedPitches[0] : 0.0f);
        }
        
        // Update current pitch (with basic smoothing)
        if (targetPitch > 0.0f)
        {
            currentPitch_ = currentPitch_ * 0.8f + targetPitch * 0.2f;
        }
        
        // Apply octave shifting to the final target frequency
        float octaveShift = octaveShiftParam_ ? octaveShiftParam_->load() : 1.0f;
        float finalTargetPitch = currentPitch_ / std::pow(2.0f, octaveShift);
        
        // Check synth mode
        InstrumentMode instrumentMode = static_cast<InstrumentMode>(instrumentModeParam_ ? static_cast<int>(instrumentModeParam_->load()) : 1);
        
        // Debug: Log synthesis parameters periodically
        if (processCounter % 100 == 0) // More frequent logging
        {
            debugLog("=== SYNTHESIS DEBUG ===");
            debugLog("Final Target Pitch: " + juce::String(finalTargetPitch, 1) + " Hz");
            debugLog("Current Pitch: " + juce::String(currentPitch_, 1) + " Hz");
            debugLog("Octave Shift: " + juce::String(octaveShift, 1) + " octaves");
            debugLog("Instrument Mode: " + juce::String(static_cast<int>(instrumentMode)));
            debugLog("Input Source: " + juce::String(inputTestEnabled ? "Test Tone" : "Live Audio"));
        }
        
        // Configure synthesizer
        instrumentSynthesizer_->setFrequency(finalTargetPitch);
        
        // Only produce output if we have detected pitch and input audio
        bool hasInputAudio = boostedInputRMS > 0.001f;
        bool hasValidPitch = currentPitch_ > 0.0f;
        
        if (hasInputAudio && hasValidPitch)
        {
            instrumentSynthesizer_->setAmplitude(0.3f);
        }
        else
        {
            // No output when no input or no pitch detected
            instrumentSynthesizer_->setAmplitude(0.0f);
            
            // Reset synthesizer when no input to ensure clean state
            if (!hasInputAudio)
            {
                instrumentSynthesizer_->reset();
            }
        }
        
        instrumentSynthesizer_->setInstrumentMode(instrumentMode);
        
        // Generate bass output
        for (int channel = 0; channel < totalNumOutputChannels; ++channel)
        {
            auto* outputData = buffer.getWritePointer(channel);
            if (outputData == nullptr)
            {
                debugLog("ERROR: Output data pointer is null for channel " + juce::String(channel));
                continue;
            }
            instrumentSynthesizer_->renderBlock(outputData, numSamples);
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
                     ", InstrumentSynthesizer: " + juce::String(instrumentSynthesizer_ ? "OK" : "NULL"));
        }
    }
    
    // Generate MIDI output when in piano mode
    generateMidiOutput(midiMessages, buffer.getNumSamples());
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
// MIDI output functionality
void GuitarToBassAudioProcessor::generateMidiOutput(juce::MidiBuffer& midiBuffer, [[maybe_unused]] int numSamples)
{
    // Only generate MIDI when in piano mode
    InstrumentMode instrumentMode = static_cast<InstrumentMode>(instrumentModeParam_ ? static_cast<int>(instrumentModeParam_->load()) : 1);
    if (instrumentMode != InstrumentMode::Piano)
    {
        // If not in piano mode and we have a note on, turn it off
        if (midiNoteOn_)
        {
            auto noteOffMessage = juce::MidiMessage::noteOff(1, currentMidiNote_);
            midiBuffer.addEvent(noteOffMessage, 0);
            midiNoteOn_ = false;
            currentMidiNote_ = -1;
        }
        return;
    }
    
    // Determine target MIDI note based on detected pitch
    int targetMidiNote = -1;
    float noteVelocity = 0.0f;
    
    // Use chord root if available and stable, otherwise use current pitch
    if (currentChord_.isValid() && currentChord_.isStable)
    {
        targetMidiNote = currentChord_.rootNote.midiNote;
        noteVelocity = std::min(127.0f, currentChord_.confidence * 100.0f + 60.0f); // Scale confidence to velocity
    }
    else if (currentPitch_ > 0.0f)
    {
        targetMidiNote = NoteDetector::frequencyToMidiNote(currentPitch_);
        noteVelocity = 80.0f; // Default velocity
    }
    
    // Check if we need to change the current MIDI note
    bool shouldPlayNote = (targetMidiNote >= 0) && (getInputLevel() > 0.001f); // Only play if we have audio input
    
    if (shouldPlayNote && targetMidiNote != currentMidiNote_)
    {
        // Turn off the current note if one is playing
        if (midiNoteOn_)
        {
            auto noteOffMessage = juce::MidiMessage::noteOff(1, currentMidiNote_);
            midiBuffer.addEvent(noteOffMessage, 0);
        }
        
        // Turn on the new note
        currentMidiNote_ = targetMidiNote;
        midiNoteVelocity_ = noteVelocity;
        auto noteOnMessage = juce::MidiMessage::noteOn(1, currentMidiNote_, static_cast<juce::uint8>(midiNoteVelocity_));
        midiBuffer.addEvent(noteOnMessage, 0);
        midiNoteOn_ = true;
        
        // Debug log MIDI events
        static int midiCounter = 0;
        if (++midiCounter % 10 == 0) // Log every 10th MIDI event
        {
            std::string noteName = NoteDetector::midiNoteToNoteName(currentMidiNote_);
            debugLog("MIDI Note On: " + juce::String(noteName) + " (MIDI " + juce::String(currentMidiNote_) + 
                     "), Velocity: " + juce::String(static_cast<int>(midiNoteVelocity_)));
        }
    }
    else if (!shouldPlayNote && midiNoteOn_)
    {
        // Turn off the current note if we should stop playing
        auto noteOffMessage = juce::MidiMessage::noteOff(1, currentMidiNote_);
        midiBuffer.addEvent(noteOffMessage, 0);
        midiNoteOn_ = false;
        currentMidiNote_ = -1;
        
        debugLog("MIDI Note Off");
    }
}

//==============================================================================
// Note-based detection getters
ChordRootDetector::ChordInfo GuitarToBassAudioProcessor::getCurrentChord() const
{
    return currentChord_;
}

BassNoteMapper::BassNote GuitarToBassAudioProcessor::getCurrentBassNote() const
{
    return currentBassNote_;
}

std::vector<NoteDetector::Note> GuitarToBassAudioProcessor::getDetectedNotes() const
{
    return currentDetectedNotes_;
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GuitarToBassAudioProcessor();
}
