#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Test class for YIN algorithm implementation
class YINPitchDetector {
public:
    YINPitchDetector(int bufferSize, float sampleRate) 
        : bufferSize_(bufferSize), sampleRate_(sampleRate) {}
    
    float detectPitch(const std::vector<float>& audioBuffer) {
        // This will be implemented - for now return 0
        return 0.0f;
    }
    
private:
    int bufferSize_;
    float sampleRate_;
};

class YINAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        sampleRate = 44100.0f;
        bufferSize = 1024;
        detector = std::make_unique<YINPitchDetector>(bufferSize, sampleRate);
    }
    
    std::vector<float> generateSineWave(float frequency, int numSamples) {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] = std::sin(2.0f * M_PI * frequency * i / sampleRate);
        }
        return buffer;
    }
    
    float sampleRate;
    int bufferSize;
    std::unique_ptr<YINPitchDetector> detector;
};

TEST_F(YINAlgorithmTest, DetectsLowEString) {
    // Low E string = 82.41 Hz
    auto sineWave = generateSineWave(82.41f, bufferSize);
    float detectedPitch = detector->detectPitch(sineWave);
    
    // Should detect within 5 cents accuracy (about 0.3% tolerance)
    EXPECT_NEAR(detectedPitch, 82.41f, 0.25f);
}

TEST_F(YINAlgorithmTest, DetectsAString) {
    // A string = 110 Hz  
    auto sineWave = generateSineWave(110.0f, bufferSize);
    float detectedPitch = detector->detectPitch(sineWave);
    
    EXPECT_NEAR(detectedPitch, 110.0f, 0.33f);
}

TEST_F(YINAlgorithmTest, DetectsDString) {
    // D string = 146.83 Hz
    auto sineWave = generateSineWave(146.83f, bufferSize);
    float detectedPitch = detector->detectPitch(sineWave);
    
    EXPECT_NEAR(detectedPitch, 146.83f, 0.44f);
}

TEST_F(YINAlgorithmTest, ReturnsZeroForSilence) {
    std::vector<float> silence(bufferSize, 0.0f);
    float detectedPitch = detector->detectPitch(silence);
    
    EXPECT_EQ(detectedPitch, 0.0f);
}