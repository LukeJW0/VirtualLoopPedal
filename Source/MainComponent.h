#pragma once

#include <JuceHeader.h>
#include <array>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent  : public juce::AudioAppComponent,
                       private juce::Timer
{
public:
    //==============================================================================
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    //==============================================================================
    static constexpr int maxTracks = 8;
    static constexpr int waveformBinCount = 96;

    void configureControls();
    void configureTrackRows();
    void updateTransportState();
    void updateTrackDisplay();
    void timerCallback() override;
    void drawWaveformTrack (juce::Graphics& g, int trackIndex);
    void startRecording();
    void finishRecording();
    bool startNextEmptyTrack();
    int getSelectedLoopBars() const;
    int getLoopLengthInSamples() const;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label tempoLabel;
    juce::Label loopLengthLabel;
    juce::Label statusLabel;

    juce::Slider tempoSlider;
    juce::ComboBox loopLengthBox;
    juce::ToggleButton countInToggle;
    juce::ToggleButton metronomeToggle;
    juce::TextButton recordButton;
    juce::TextButton addTrackButton;
    juce::TextButton stopButton;

    std::array<juce::Label, maxTracks> trackNameLabels;
    std::array<juce::Label, maxTracks> trackStatusLabels;
    std::array<juce::TextButton, maxTracks> trackMuteButtons;
    std::array<bool, maxTracks> trackHasRecording {};
    std::array<juce::Rectangle<int>, maxTracks> trackWaveformBounds;
    std::array<std::array<float, waveformBinCount>, maxTracks> trackWaveforms {};

    bool isRecording = false;
    bool isPlaying = false;
    int currentTrackIndex = 0;
    int activeTrackCount = 1;
    int recordingSamplePosition = 0;
    int recordingLoopSamples = 1;
    double currentSampleRate = 44100.0;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
