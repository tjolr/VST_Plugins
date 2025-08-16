/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
GuitarToBassAudioProcessorEditor::GuitarToBassAudioProcessorEditor (GuitarToBassAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Set up title label
    titleLabel.setText("Guitar to Bass v3", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(24.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);
    
    // Set up pitch display
    pitchDisplayLabel.setText("Pitch: --", juce::dontSendNotification);
    pitchDisplayLabel.setFont(juce::FontOptions(16.0f));
    pitchDisplayLabel.setJustificationType(juce::Justification::centred);
    pitchDisplayLabel.setColour(juce::Label::textColourId, juce::Colours::lightblue);
    addAndMakeVisible(pitchDisplayLabel);
    
    // Set up octave slider
    octaveSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    octaveSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    octaveSlider.setColour(juce::Slider::thumbColourId, juce::Colours::orange);
    octaveSlider.setColour(juce::Slider::trackColourId, juce::Colours::darkgrey);
    addAndMakeVisible(octaveSlider);
    
    octaveLabel.setText("Octave Shift", juce::dontSendNotification);
    octaveLabel.setFont(juce::FontOptions(14.0f));
    octaveLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    octaveLabel.attachToComponent(&octaveSlider, true);
    addAndMakeVisible(octaveLabel);
    
    // Set up synth mode toggle
    synthModeToggle.setButtonText("Synth");
    synthModeToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    synthModeToggle.setColour(juce::ToggleButton::tickColourId, juce::Colours::green);
    addAndMakeVisible(synthModeToggle);
    
    synthModeLabel.setText("Bass Mode", juce::dontSendNotification);
    synthModeLabel.setFont(juce::FontOptions(14.0f));
    synthModeLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    synthModeLabel.attachToComponent(&synthModeToggle, true);
    addAndMakeVisible(synthModeLabel);
    
    // Set up input test button
    inputTestButton.setButtonText("Test Input");
    inputTestButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    inputTestButton.setColour(juce::ToggleButton::tickColourId, juce::Colours::red);
    addAndMakeVisible(inputTestButton);
    
    // Set up level meter labels
    inputMeterLabel.setText("Input", juce::dontSendNotification);
    inputMeterLabel.setFont(juce::FontOptions(12.0f));
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    inputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputMeterLabel);
    
    outputMeterLabel.setText("Output", juce::dontSendNotification);
    outputMeterLabel.setFont(juce::FontOptions(12.0f));
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    outputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputMeterLabel);
    
    // Create parameter attachments
    auto& params = audioProcessor.getParameters();
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params, "octaveShift", octaveSlider);
    synthModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params, "synthMode", synthModeToggle);
    inputTestAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params, "inputTest", inputTestButton);
    
    // Start timer for pitch display updates
    startTimerHz(30);
    
    setSize (500, 200);
}

GuitarToBassAudioProcessorEditor::~GuitarToBassAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void GuitarToBassAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Modern gradient background
    juce::ColourGradient gradient(juce::Colours::darkslategrey, 0, 0,
                                  juce::Colours::black, 0, static_cast<float>(getHeight()), false);
    g.setGradientFill(gradient);
    g.fillAll();
    
    // Draw border
    g.setColour(juce::Colours::darkgrey);
    g.drawRect(getLocalBounds(), 2);
    
    // Draw level meters
    drawLevelMeter(g, inputMeterBounds, audioProcessor.getInputLevel(), juce::Colours::green);
    drawLevelMeter(g, outputMeterBounds, audioProcessor.getOutputLevel(), juce::Colours::orange);
}

void GuitarToBassAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(16, 16);
    
    // Title at the top
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(10);
    
    // Pitch display
    pitchDisplayLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(20);
    
    // Octave slider
    auto octaveArea = bounds.removeFromTop(30);
    octaveArea.removeFromLeft(100); // Space for label
    octaveSlider.setBounds(octaveArea);
    bounds.removeFromTop(10);
    
    // Synth mode toggle
    auto synthArea = bounds.removeFromTop(30);
    synthArea.removeFromLeft(100); // Space for label
    synthModeToggle.setBounds(synthArea.removeFromLeft(80));
    bounds.removeFromTop(10);
    
    // Input test button
    auto testArea = bounds.removeFromTop(30);
    inputTestButton.setBounds(testArea.removeFromLeft(120));
    bounds.removeFromTop(15);
    
    // Level meters at the bottom
    auto meterArea = bounds.removeFromBottom(60);
    
    // Input meter
    auto inputArea = meterArea.removeFromLeft(meterArea.getWidth() / 2 - 10);
    inputMeterLabel.setBounds(inputArea.removeFromTop(20));
    inputMeterBounds = inputArea.toFloat();
    
    meterArea.removeFromLeft(20); // Gap between meters
    
    // Output meter
    auto outputArea = meterArea;
    outputMeterLabel.setBounds(outputArea.removeFromTop(20));
    outputMeterBounds = outputArea.toFloat();
}

void GuitarToBassAudioProcessorEditor::timerCallback()
{
    // Update pitch display with current detected pitch
    float currentPitch = audioProcessor.getCurrentPitch();
    if (currentPitch > 0.0f)
    {
        pitchDisplayLabel.setText("Pitch: " + juce::String(currentPitch, 1) + " Hz", 
                                  juce::dontSendNotification);
    }
    else
    {
        pitchDisplayLabel.setText("Pitch: --", juce::dontSendNotification);
    }
    
    // Trigger repaint for level meters
    repaint();
}

void GuitarToBassAudioProcessorEditor::drawLevelMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float level, juce::Colour colour)
{
    // Background
    g.setColour(juce::Colours::darkgrey);
    g.fillRect(bounds);
    
    // Convert RMS level to dB and normalize for display
    float dbLevel = level > 0.0f ? 20.0f * std::log10(level) : -60.0f;
    float normalizedLevel = juce::jlimit(0.0f, 1.0f, (dbLevel + 60.0f) / 60.0f); // -60dB to 0dB range
    
    // Level bar
    auto levelBounds = bounds;
    levelBounds.setHeight(bounds.getHeight() * normalizedLevel);
    levelBounds.setY(bounds.getBottom() - levelBounds.getHeight());
    
    // Color gradient based on level
    juce::Colour levelColour = colour;
    if (normalizedLevel > 0.8f) // Red zone for high levels
        levelColour = juce::Colours::red;
    else if (normalizedLevel > 0.6f) // Yellow zone for medium levels
        levelColour = juce::Colours::yellow;
    
    g.setColour(levelColour);
    g.fillRect(levelBounds);
    
    // Border
    g.setColour(juce::Colours::white);
    g.drawRect(bounds, 1);
}
