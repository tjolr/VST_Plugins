/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

// Debug logging function for editor
static void debugLogEditor(const juce::String& message)
{
    juce::Logger::writeToLog("GuitarToBass Editor: " + message);
}

//==============================================================================
GuitarToBassAudioProcessorEditor::GuitarToBassAudioProcessorEditor (GuitarToBassAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    debugLogEditor("PluginEditor constructor called");
    
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
    
    // Set up debug log display
    debugLogLabel.setText("Debug: Waiting for audio...", juce::dontSendNotification);
    debugLogLabel.setFont(juce::FontOptions(11.0f));
    debugLogLabel.setJustificationType(juce::Justification::centred);
    debugLogLabel.setColour(juce::Label::textColourId, juce::Colours::yellow);
    debugLogLabel.setTooltip("Debug information - Check console for detailed logs");
    addAndMakeVisible(debugLogLabel);
    
    // Add mouse listener to debug label
    debugLogLabel.addMouseListener(this, false);
    
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
    
    // Set up gate threshold slider
    gateThresholdSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gateThresholdSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);
    gateThresholdSlider.setColour(juce::Slider::thumbColourId, juce::Colours::purple);
    gateThresholdSlider.setColour(juce::Slider::trackColourId, juce::Colours::darkgrey);
    addAndMakeVisible(gateThresholdSlider);
    
    gateThresholdLabel.setText("Gate Threshold", juce::dontSendNotification);
    gateThresholdLabel.setFont(juce::FontOptions(14.0f));
    gateThresholdLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    gateThresholdLabel.attachToComponent(&gateThresholdSlider, true);
    addAndMakeVisible(gateThresholdLabel);
    
    // Add button listener to test UI interaction
    inputTestButton.onClick = []() {
        debugLogEditor("=== TEST INPUT BUTTON CLICKED! ===");
        debugLogEditor("UI interaction is working - looking for audio engine controls...");
        
        // Show a popup to confirm the button click
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Button Test",
                                               "Test Input button was clicked!\n\nNow look for the 'Options' button to start the audio engine.",
                                               "OK");
    };
    
    // Add a helpful button to enable live input
    enableLiveInputButton.setButtonText("Enable Live Input");
    enableLiveInputButton.setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    enableLiveInputButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(enableLiveInputButton);
    
    enableLiveInputButton.onClick = []() {
        debugLogEditor("=== LIVE INPUT ENABLE INSTRUCTIONS ===");
        
        juce::String instructions = "To enable live input:\n\n";
        instructions += "1. Click the 'Settings...' button in the yellow banner\n";
        instructions += "2. In the Audio/MIDI Settings dialog:\n";
        instructions += "   - Uncheck 'Mute audio input'\n";
        instructions += "   - Select your input device\n";
        instructions += "   - Select your output device\n";
        instructions += "3. Click OK to apply settings\n\n";
        instructions += "This will allow your guitar/microphone to be processed in real-time!";
        
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Enable Live Input",
                                               instructions,
                                               "OK");
    };
    
    // Add a manual audio engine start button
    auto* startAudioButton = new juce::TextButton("Start Audio Engine");
    startAudioButton->setButtonText("Start Audio Engine");
    startAudioButton->setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    startAudioButton->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    addAndMakeVisible(startAudioButton);
    
    startAudioButton->onClick = [this]() {
        debugLogEditor("=== MANUAL AUDIO ENGINE START REQUESTED ===");
        debugLogEditor("Attempting to force audio engine start...");
        
        // Try to trigger audio processing by enabling test input
        if (inputTestParam_)
        {
            debugLogEditor("Enabling test input to trigger audio processing...");
            inputTestParam_->store(1.0f);
        }
        
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Audio Engine Start",
                                               "Audio engine start requested.\n\nCheck console for debug logs.",
                                               "OK");
    };
    
    // Set up level meter labels with enhanced styling
    inputMeterLabel.setText("INPUT LEVEL", juce::dontSendNotification);
    inputMeterLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    inputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputMeterLabel);
    
    outputMeterLabel.setText("OUTPUT LEVEL", juce::dontSendNotification);
    outputMeterLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    outputMeterLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputMeterLabel);
    
    // Set up dB display labels
    inputDbLabel.setText("-∞ dB", juce::dontSendNotification);
    inputDbLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    inputDbLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    inputDbLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(inputDbLabel);
    
    outputDbLabel.setText("-∞ dB", juce::dontSendNotification);
    outputDbLabel.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    outputDbLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    outputDbLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(outputDbLabel);
    
    // Create parameter attachments
    auto& params = audioProcessor.getParameters();
    octaveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params, "octaveShift", octaveSlider);
    synthModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params, "synthMode", synthModeToggle);
    inputTestAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(params, "inputTest", inputTestButton);
    gateThresholdAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(params, "gateThreshold", gateThresholdSlider);
    
    // Get parameter pointers for direct access
    inputTestParam_ = params.getRawParameterValue("inputTest");
    
    // Start timer for pitch display updates
    startTimerHz(60); // Increased from 30Hz to 60Hz for smoother level meter updates
    
    debugLogEditor("Editor setup complete");
    
    setSize (600, 520); // Increased height from 420 to 520 for better spacing
}

GuitarToBassAudioProcessorEditor::~GuitarToBassAudioProcessorEditor()
{
    stopTimer();
    debugLogLabel.removeMouseListener(this);
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
    bounds.reduce(16, 20); // Increased bottom margin
    
    // Title at the top
    titleLabel.setBounds(bounds.removeFromTop(40));
    bounds.removeFromTop(20); // Increased from 15 to 20
    
    // Pitch display
    pitchDisplayLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(30); // Increased from 25 to 30
    
    // Debug log display
    debugLogLabel.setBounds(bounds.removeFromTop(30));
    bounds.removeFromTop(20); // Increased from 15 to 20
    
    // Octave slider
    auto octaveArea = bounds.removeFromTop(30);
    octaveArea.removeFromLeft(100); // Space for label
    octaveSlider.setBounds(octaveArea);
    bounds.removeFromTop(20); // Increased from 15 to 20
    
    // Synth mode toggle
    auto synthArea = bounds.removeFromTop(30);
    synthArea.removeFromLeft(100); // Space for label
    synthModeToggle.setBounds(synthArea.removeFromLeft(80));
    bounds.removeFromTop(20); // Increased from 15 to 20
    
    // Gate threshold slider
    auto gateArea = bounds.removeFromTop(30);
    gateArea.removeFromLeft(100); // Space for label
    gateThresholdSlider.setBounds(gateArea);
    bounds.removeFromTop(20); // Increased from 15 to 20
    
    // Input test button
    auto testArea = bounds.removeFromTop(30);
    inputTestButton.setBounds(testArea.removeFromLeft(120));
    bounds.removeFromTop(15); // Increased from 10 to 15
    
    // Enable live input button
    auto liveInputArea = bounds.removeFromTop(30);
    enableLiveInputButton.setBounds(liveInputArea.removeFromLeft(150));
    bounds.removeFromTop(25); // Increased from 20 to 25
    
    // Level meters at the bottom - made larger and more prominent
    auto meterArea = bounds.removeFromBottom(180); // Increased from 150 to 180 for better meter visibility
    
    // Input meter
    auto inputArea = meterArea.removeFromLeft(meterArea.getWidth() / 2 - 15);
    inputMeterLabel.setBounds(inputArea.removeFromTop(25));
    inputMeterBounds = inputArea.toFloat();
    
    // Input dB label positioned below the meter
    inputDbLabel.setBounds(inputArea.getX(), inputArea.getBottom() + 5, inputArea.getWidth(), 20);
    
    meterArea.removeFromLeft(30); // Gap between meters
    
    // Output meter
    auto outputArea = meterArea;
    outputMeterLabel.setBounds(outputArea.removeFromTop(25));
    outputMeterBounds = outputArea.toFloat();
    
    // Output dB label positioned below the meter
    outputDbLabel.setBounds(outputArea.getX(), outputArea.getBottom() + 5, outputArea.getWidth(), 20);
    
    // Add extra bottom padding to ensure dB labels are visible
    bounds.removeFromBottom(10);
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
    
    // Update dB labels
    float inputLevel = audioProcessor.getInputLevel();
    float outputLevel = audioProcessor.getOutputLevel();
    
    float inputDb = inputLevel > 0.0f ? 20.0f * std::log10(inputLevel) : -60.0f;
    float outputDb = outputLevel > 0.0f ? 20.0f * std::log10(outputLevel) : -60.0f;
    
    inputDbLabel.setText(juce::String(inputDb, 1) + " dB", juce::dontSendNotification);
    outputDbLabel.setText(juce::String(outputDb, 1) + " dB", juce::dontSendNotification);
    
    // Update debug display with comprehensive information
    static int debugCounter = 0;
    if (++debugCounter % 60 == 0) // Update debug display every second (60 Hz timer)
    {
        juce::String debugInfo = "Input: " + juce::String(inputDb, 1) + " dB";
        debugInfo += " | Output: " + juce::String(outputDb, 1) + " dB";
        debugInfo += " | Pitch: " + (currentPitch > 0.0f ? juce::String(currentPitch, 1) + " Hz" : "None");
        
        // Add audio activity indicators
        bool hasInputAudio = inputLevel > 0.001f;
        bool hasOutputAudio = outputLevel > 0.001f;
        bool hasPitch = currentPitch > 0.0f;
        
        debugInfo += " | Audio: " + juce::String(hasInputAudio ? "IN" : "NO_IN") + "/" + 
                     juce::String(hasOutputAudio ? "OUT" : "NO_OUT") + "/" + 
                     juce::String(hasPitch ? "PITCH" : "NO_PITCH");
        
        debugLogLabel.setText(debugInfo, juce::dontSendNotification);
        
        // Log to console for detailed debugging
        debugLogEditor("Status - Input: " + juce::String(inputDb, 1) + " dB (" + 
                       juce::String(hasInputAudio ? "ACTIVE" : "SILENT") + "), " +
                       "Output: " + juce::String(outputDb, 1) + " dB (" + 
                       juce::String(hasOutputAudio ? "ACTIVE" : "SILENT") + "), " +
                       "Pitch: " + (currentPitch > 0.0f ? juce::String(currentPitch, 1) + " Hz" : "None"));
        
        // Debug: Check if input level is changing
        static float lastInputLevel = 0.0f;
        if (std::abs(inputLevel - lastInputLevel) > 0.0001f)
        {
            debugLogEditor("INPUT LEVEL CHANGED: " + juce::String(lastInputLevel, 6) + " -> " + juce::String(inputLevel, 6));
            lastInputLevel = inputLevel;
        }
    }
    
    // Trigger repaint for level meters
    repaint();
}

void GuitarToBassAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    // If debug label was clicked, show detailed debug info
    if (event.eventComponent == &debugLogLabel)
    {
        float inputLevel = audioProcessor.getInputLevel();
        float outputLevel = audioProcessor.getOutputLevel();
        float currentPitch = audioProcessor.getCurrentPitch();
        
        juce::String detailedInfo = "=== DETAILED DEBUG INFO ===\n";
        detailedInfo += "Input Level: " + juce::String(inputLevel, 6) + " (" + 
                       juce::String(20.0f * std::log10(inputLevel > 0.0f ? inputLevel : 0.000001f), 1) + " dB)\n";
        detailedInfo += "Output Level: " + juce::String(outputLevel, 6) + " (" + 
                       juce::String(20.0f * std::log10(outputLevel > 0.0f ? outputLevel : 0.000001f), 1) + " dB)\n";
        detailedInfo += "Current Pitch: " + (currentPitch > 0.0f ? juce::String(currentPitch, 1) + " Hz" : "None") + "\n";
        detailedInfo += "Sample Rate: " + juce::String(audioProcessor.getSampleRate()) + " Hz\n";
        detailedInfo += "Block Size: " + juce::String(audioProcessor.getBlockSize()) + " samples\n";
        detailedInfo += "Input Channels: " + juce::String(audioProcessor.getTotalNumInputChannels()) + "\n";
        detailedInfo += "Output Channels: " + juce::String(audioProcessor.getTotalNumOutputChannels()) + "\n";
        detailedInfo += "Check console for detailed processing logs";
        
        debugLogEditor("=== DETAILED DEBUG INFO ===");
        debugLogEditor("Input Level: " + juce::String(inputLevel, 6) + " (" + 
                      juce::String(20.0f * std::log10(inputLevel > 0.0f ? inputLevel : 0.000001f), 1) + " dB)");
        debugLogEditor("Output Level: " + juce::String(outputLevel, 6) + " (" + 
                      juce::String(20.0f * std::log10(outputLevel > 0.0f ? outputLevel : 0.000001f), 1) + " dB)");
        debugLogEditor("Current Pitch: " + (currentPitch > 0.0f ? juce::String(currentPitch, 1) + " Hz" : "None"));
        debugLogEditor("Sample Rate: " + juce::String(audioProcessor.getSampleRate()) + " Hz");
        debugLogEditor("Block Size: " + juce::String(audioProcessor.getBlockSize()) + " samples");
        debugLogEditor("Input Channels: " + juce::String(audioProcessor.getTotalNumInputChannels()));
        debugLogEditor("Output Channels: " + juce::String(audioProcessor.getTotalNumOutputChannels()));
        
        // Show alert with detailed info
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                              "Debug Information",
                                              detailedInfo,
                                              "OK");
    }
}

void GuitarToBassAudioProcessorEditor::drawLevelMeter(juce::Graphics& g, const juce::Rectangle<float>& bounds, float level, juce::Colour colour)
{
    // Background with subtle gradient
    juce::ColourGradient bgGradient(juce::Colours::darkgrey.darker(0.3f), bounds.getX(), bounds.getY(),
                                    juce::Colours::darkgrey, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(bgGradient);
    g.fillRect(bounds);
    
    // Convert RMS level to dB and normalize for display
    float dbLevel = level > 0.0f ? 20.0f * std::log10(level) : -60.0f;
    float normalizedLevel = juce::jlimit(0.0f, 1.0f, (dbLevel + 60.0f) / 60.0f); // -60dB to 0dB range
    
    // Level bar with gradient
    auto levelBounds = bounds;
    levelBounds.setHeight(bounds.getHeight() * normalizedLevel);
    levelBounds.setY(bounds.getBottom() - levelBounds.getHeight());
    
    // Color gradient based on level
    juce::Colour levelColour = colour;
    if (normalizedLevel > 0.8f) // Red zone for high levels
        levelColour = juce::Colours::red;
    else if (normalizedLevel > 0.6f) // Yellow zone for medium levels
        levelColour = juce::Colours::yellow;
    else if (normalizedLevel > 0.3f) // Green zone for low levels
        levelColour = juce::Colours::green;
    
    // Create gradient for the level bar
    juce::ColourGradient levelGradient(levelColour.brighter(0.3f), levelBounds.getX(), levelBounds.getY(),
                                       levelColour, levelBounds.getX(), levelBounds.getBottom(), false);
    g.setGradientFill(levelGradient);
    g.fillRect(levelBounds);
    
    // Add subtle highlight at the top of the level bar
    if (normalizedLevel > 0.1f)
    {
        auto highlightBounds = levelBounds;
        highlightBounds.setHeight(2.0f);
        g.setColour(levelColour.brighter(0.5f));
        g.fillRect(highlightBounds);
    }
    
    // Draw level markers (dB scale)
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.setFont(juce::FontOptions(10.0f));
    
    // Draw -20dB, -10dB, -6dB, -3dB, 0dB markers
    std::vector<float> dbMarkers = {-20.0f, -10.0f, -6.0f, -3.0f, 0.0f};
    for (float db : dbMarkers)
    {
        float markerPos = (db + 60.0f) / 60.0f; // Convert to 0-1 range
        markerPos = juce::jlimit(0.0f, 1.0f, markerPos);
        float yPos = bounds.getBottom() - (bounds.getHeight() * markerPos);
        
        // Draw marker line
        g.drawHorizontalLine(static_cast<int>(yPos), bounds.getX(), bounds.getRight());
        
        // Draw dB label
        juce::String dbText = juce::String(static_cast<int>(db));
        g.drawText(dbText, bounds.getRight() + 2, static_cast<int>(yPos - 6), 25, 12, juce::Justification::left);
    }
    
    // Border with subtle glow effect
    g.setColour(juce::Colours::white.withAlpha(0.5f));
    g.drawRect(bounds, 1);
    
    // Add peak indicator if level is high
    if (normalizedLevel > 0.9f)
    {
        g.setColour(juce::Colours::red);
        g.fillEllipse(bounds.getRight() - 8, bounds.getY() + 2, 6, 6);
    }
}
