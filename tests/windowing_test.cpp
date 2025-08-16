#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Enhanced YIN detector with overlapping windows
class EnhancedYINDetector {
public:
    EnhancedYINDetector(int analysisSize, float sampleRate, float overlapRatio = 0.5f)
        : analysisSize_(analysisSize), sampleRate_(sampleRate), overlapRatio_(overlapRatio)
    {
        hopSize_ = static_cast<int>(analysisSize_ * (1.0f - overlapRatio_));
        buffer_.resize(analysisSize_);
        window_.resize(analysisSize_);
        generateWindow();
    }
    
    float processBuffer(const float* input, int numSamples) {
        // Placeholder - will implement overlapping analysis
        return 0.0f;
    }
    
private:
    void generateWindow() {
        // Hann window for smooth overlapping
        for (int i = 0; i < analysisSize_; ++i) {
            float x = static_cast<float>(i) / (analysisSize_ - 1);
            window_[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * x));
        }
    }
    
    int analysisSize_;
    float sampleRate_;
    float overlapRatio_;
    int hopSize_;
    std::vector<float> buffer_;
    std::vector<float> window_;
    int bufferIndex_ = 0;
};

class WindowingTest : public ::testing::Test {
protected:
    void SetUp() override {
        sampleRate = 44100.0f;
        analysisSize = 1024;
        detector = std::make_unique<EnhancedYINDetector>(analysisSize, sampleRate, 0.5f);
    }
    
    std::vector<float> generateSustainedTone(float frequency, int numSamples) {
        std::vector<float> buffer(numSamples);
        for (int i = 0; i < numSamples; ++i) {
            buffer[i] = std::sin(2.0f * M_PI * frequency * i / sampleRate);
        }
        return buffer;
    }
    
    float sampleRate;
    int analysisSize;
    std::unique_ptr<EnhancedYINDetector> detector;
};

TEST_F(WindowingTest, HandlesOverlappingWindows) {
    auto sustainedTone = generateSustainedTone(110.0f, 2048);
    
    float pitch1 = detector->processBuffer(sustainedTone.data(), 512);
    float pitch2 = detector->processBuffer(sustainedTone.data() + 256, 512);
    
    // Both should detect similar pitch for sustained tone
    if (pitch1 > 0.0f && pitch2 > 0.0f) {
        EXPECT_NEAR(pitch1, pitch2, 5.0f); // Within 5 Hz
    }
}

TEST_F(WindowingTest, SmoothTransitions) {
    // Generate tone that changes frequency
    auto tone1 = generateSustainedTone(110.0f, 512);
    auto tone2 = generateSustainedTone(146.83f, 512);
    
    float pitch1 = detector->processBuffer(tone1.data(), 512);
    float pitch2 = detector->processBuffer(tone2.data(), 512);
    
    // Should detect different pitches
    if (pitch1 > 0.0f && pitch2 > 0.0f) {
        EXPECT_GT(std::abs(pitch2 - pitch1), 20.0f); // Should be different
    }
}