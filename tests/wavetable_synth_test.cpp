#include <gtest/gtest.h>
#include <vector>
#include <cmath>

// Forward declaration of the bass synthesizer
class BassSynthesizer {
public:
    BassSynthesizer(float sampleRate) : sampleRate_(sampleRate) {}
    
    void setFrequency(float frequency) { frequency_ = frequency; }
    void setAmplitude(float amplitude) { amplitude_ = amplitude; }
    void renderBlock(float* output, int numSamples) {
        // Placeholder implementation - will be implemented
        for (int i = 0; i < numSamples; ++i) {
            output[i] = 0.0f;
        }
    }
    
private:
    float sampleRate_;
    float frequency_ = 440.0f;
    float amplitude_ = 0.5f;
};

class WavetableSynthTest : public ::testing::Test {
protected:
    void SetUp() override {
        sampleRate = 44100.0f;
        synthesizer = std::make_unique<BassSynthesizer>(sampleRate);
        bufferSize = 512;
        buffer.resize(bufferSize);
    }
    
    float calculateRMS(const std::vector<float>& buffer) {
        float sum = 0.0f;
        for (float sample : buffer) {
            sum += sample * sample;
        }
        return std::sqrt(sum / buffer.size());
    }
    
    float sampleRate;
    int bufferSize;
    std::vector<float> buffer;
    std::unique_ptr<BassSynthesizer> synthesizer;
};

TEST_F(WavetableSynthTest, ProducesAudioOutput) {
    synthesizer->setFrequency(82.41f); // Low E
    synthesizer->setAmplitude(0.5f);
    
    synthesizer->renderBlock(buffer.data(), bufferSize);
    
    float rms = calculateRMS(buffer);
    EXPECT_GT(rms, 0.01f); // Should produce some audio output
}

TEST_F(WavetableSynthTest, SilentWhenZeroAmplitude) {
    synthesizer->setFrequency(110.0f); // A
    synthesizer->setAmplitude(0.0f);
    
    synthesizer->renderBlock(buffer.data(), bufferSize);
    
    float rms = calculateRMS(buffer);
    EXPECT_LT(rms, 0.001f); // Should be essentially silent
}

TEST_F(WavetableSynthTest, HandlesLowBassFrequencies) {
    synthesizer->setFrequency(41.2f); // Low E one octave down
    synthesizer->setAmplitude(0.5f);
    
    synthesizer->renderBlock(buffer.data(), bufferSize);
    
    float rms = calculateRMS(buffer);
    EXPECT_GT(rms, 0.01f); // Should handle low frequencies
}