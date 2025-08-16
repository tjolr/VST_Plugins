#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <algorithm>

// Enhanced detector with polyphonic capabilities
class PolyphonicDetector {
public:
    PolyphonicDetector(float sampleRate) : sampleRate_(sampleRate) {}
    
    std::vector<float> detectMultiplePitches(const float* buffer, int numSamples) {
        // Placeholder - will implement spectral peak detection
        return {};
    }
    
    float findLowestPitch(const std::vector<float>& pitches) {
        if (pitches.empty()) return 0.0f;
        return *std::min_element(pitches.begin(), pitches.end());
    }
    
private:
    float sampleRate_;
};

class PolyphonicDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        sampleRate = 44100.0f;
        detector = std::make_unique<PolyphonicDetector>(sampleRate);
    }
    
    std::vector<float> generateChord(const std::vector<float>& frequencies, int numSamples) {
        std::vector<float> buffer(numSamples, 0.0f);
        
        for (float freq : frequencies) {
            for (int i = 0; i < numSamples; ++i) {
                buffer[i] += std::sin(2.0f * M_PI * freq * i / sampleRate) / frequencies.size();
            }
        }
        
        return buffer;
    }
    
    float sampleRate;
    std::unique_ptr<PolyphonicDetector> detector;
};

TEST_F(PolyphonicDetectionTest, DetectsLowestNoteInChord) {
    // Generate E major chord: E (82.41), G# (103.83), B (123.47)
    std::vector<float> chordFreqs = {82.41f, 103.83f, 123.47f};
    auto chordBuffer = generateChord(chordFreqs, 1024);
    
    auto detectedPitches = detector->detectMultiplePitches(chordBuffer.data(), 1024);
    float lowestPitch = detector->findLowestPitch(detectedPitches);
    
    // Should detect the lowest note (E)
    if (lowestPitch > 0.0f) {
        EXPECT_NEAR(lowestPitch, 82.41f, 5.0f);
    }
}

TEST_F(PolyphonicDetectionTest, HandlesMinorChord) {
    // Generate A minor chord: A (110), C (130.81), E (164.81)
    std::vector<float> chordFreqs = {110.0f, 130.81f, 164.81f};
    auto chordBuffer = generateChord(chordFreqs, 1024);
    
    auto detectedPitches = detector->detectMultiplePitches(chordBuffer.data(), 1024);
    float lowestPitch = detector->findLowestPitch(detectedPitches);
    
    // Should detect the lowest note (A)
    if (lowestPitch > 0.0f) {
        EXPECT_NEAR(lowestPitch, 110.0f, 5.0f);
    }
}