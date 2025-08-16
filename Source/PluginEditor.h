/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class GuitarToBassAudioProcessorEditor  : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    GuitarToBassAudioProcessorEditor (GuitarToBassAudioProcessor&);
    ~GuitarToBassAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    
    void timerCallback() override;
    void mouseDown(const juce::MouseEvent& event) override;
    
    void drawLevelMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float level, juce::Colour colour);

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    GuitarToBassAudioProcessor& audioProcessor;
    
    // UI Components
    juce::Slider octaveSlider;
    juce::Label octaveLabel;
    juce::ToggleButton synthModeToggle;
    juce::Label synthModeLabel;
    juce::Label titleLabel;
    juce::Label pitchDisplayLabel;
    juce::Label debugLogLabel;
    juce::ToggleButton inputTestButton;
    juce::TextButton enableLiveInputButton;
    juce::Slider gateThresholdSlider;
    juce::Label gateThresholdLabel;
    
    // Level meters
    juce::Rectangle<float> inputMeterBounds;
    juce::Rectangle<float> outputMeterBounds;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label inputDbLabel;
    juce::Label outputDbLabel;
    
    // Parameter attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> octaveAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> synthModeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> inputTestAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gateThresholdAttachment;
    
    // Parameter pointers for direct access
    std::atomic<float>* inputTestParam_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GuitarToBassAudioProcessorEditor)
};
