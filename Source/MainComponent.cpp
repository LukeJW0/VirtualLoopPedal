#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
    configureControls();
    configureTrackRows();
    updateTransportState();
    updateTrackDisplay();

    setSize (960, 640);

    // Some platforms require permissions to open input channels so request that here
    if (juce::RuntimePermissions::isRequired (juce::RuntimePermissions::recordAudio)
        && ! juce::RuntimePermissions::isGranted (juce::RuntimePermissions::recordAudio))
    {
        juce::RuntimePermissions::request (juce::RuntimePermissions::recordAudio,
                                           [&] (bool granted) { setAudioChannels (granted ? 2 : 0, 2); });
    }
    else
    {
        setAudioChannels (2, 2);
    }
}

MainComponent::~MainComponent()
{
    stopTimer();
    shutdownAudio();
}

//==============================================================================
void MainComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    juce::ignoreUnused (samplesPerBlockExpected);
    currentSampleRate = sampleRate;
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (isRecording && recordingLoopSamples > 0)
    {
        auto* buffer = bufferToFill.buffer;
        auto level = 0.0f;
        const auto channelsToRead = juce::jmin (buffer->getNumChannels(), 2);

        for (int channel = 0; channel < channelsToRead; ++channel)
            level = juce::jmax (level, buffer->getRMSLevel (channel, bufferToFill.startSample, bufferToFill.numSamples));

        const auto bin = juce::jlimit (0, waveformBinCount - 1,
                                       (recordingSamplePosition * waveformBinCount) / recordingLoopSamples);
        auto& currentWaveform = trackWaveforms[(size_t) currentTrackIndex];
        currentWaveform[(size_t) bin] = juce::jmax (currentWaveform[(size_t) bin], juce::jlimit (0.0f, 1.0f, level * 8.0f));
        recordingSamplePosition = juce::jmin (recordingSamplePosition + bufferToFill.numSamples, recordingLoopSamples);
    }

    bufferToFill.clearActiveBufferRegion();
}

void MainComponent::releaseResources()
{
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff16181d));

    auto bounds = getLocalBounds().toFloat().reduced (18.0f);

    g.setColour (juce::Colour (0xff242832));
    g.fillRoundedRectangle (bounds.removeFromTop (150.0f), 8.0f);

    bounds.removeFromTop (18.0f);

    g.setColour (juce::Colour (0xff20242c));
    g.fillRoundedRectangle (bounds, 8.0f);

    for (int i = 0; i < activeTrackCount; ++i)
        drawWaveformTrack (g, i);
}

void MainComponent::resized()
{
    auto area = getLocalBounds().reduced (30);
    auto header = area.removeFromTop (126);

    titleLabel.setBounds (header.removeFromTop (40));
    subtitleLabel.setBounds (header.removeFromTop (24));
    header.removeFromTop (10);

    auto settingsRow = header.removeFromTop (40);
    tempoLabel.setBounds (settingsRow.removeFromLeft (70));
    tempoSlider.setBounds (settingsRow.removeFromLeft (250));
    settingsRow.removeFromLeft (18);
    loopLengthLabel.setBounds (settingsRow.removeFromLeft (95));
    loopLengthBox.setBounds (settingsRow.removeFromLeft (120).reduced (0, 4));
    settingsRow.removeFromLeft (18);
    countInToggle.setBounds (settingsRow.removeFromLeft (104));
    metronomeToggle.setBounds (settingsRow.removeFromLeft (126));

    area.removeFromTop (34);
    auto transportRow = area.removeFromTop (36);
    recordButton.setBounds (transportRow.removeFromLeft (112).reduced (0, 2));
    transportRow.removeFromLeft (8);
    stopButton.setBounds (transportRow.removeFromLeft (82).reduced (0, 2));
    statusLabel.setBounds ({});
    area.removeFromTop (10);

    const auto rowHeight = 56;
    const auto columnGap = 16;
    const auto trackNameWidth = 84;
    const auto muteButtonWidth = 90;

    for (int i = 0; i < maxTracks; ++i)
    {
        if (i >= activeTrackCount)
        {
            trackNameLabels[(size_t) i].setBounds ({});
            trackStatusLabels[(size_t) i].setBounds ({});
            trackMuteButtons[(size_t) i].setBounds ({});
            continue;
        }

        auto row = area.removeFromTop (rowHeight);
        trackNameLabels[(size_t) i].setBounds (row.removeFromLeft (trackNameWidth).withTrimmedTop (8).withHeight (36));
        row.removeFromLeft (columnGap);
        trackMuteButtons[(size_t) i].setBounds (row.removeFromRight (muteButtonWidth).withTrimmedTop (8).withHeight (36));
        row.removeFromRight (columnGap);
        trackWaveformBounds[(size_t) i] = row.reduced (0, 6);
        trackStatusLabels[(size_t) i].setBounds ({});
        area.removeFromTop (8);
    }

    const auto plusButtonSize = 34;
    addTrackButton.setBounds (area.removeFromTop (plusButtonSize).withSizeKeepingCentre (plusButtonSize, plusButtonSize));
}

void MainComponent::configureControls()
{
    titleLabel.setText ("Virtual Loop Pedal", juce::dontSendNotification);
    titleLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    titleLabel.setFont (juce::Font (28.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("Tempo, loop length, count-in, and metronome settings", juce::dontSendNotification);
    subtitleLabel.setColour (juce::Label::textColourId, juce::Colour (0xffb9c0ce));
    subtitleLabel.setFont (juce::Font (15.0f));
    addAndMakeVisible (subtitleLabel);

    tempoLabel.setText ("Tempo", juce::dontSendNotification);
    tempoLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (tempoLabel);

    tempoSlider.setRange (40.0, 220.0, 1.0);
    tempoSlider.setValue (120.0);
    tempoSlider.setTextValueSuffix (" bpm");
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 24);
    tempoSlider.setColour (juce::Slider::trackColourId, juce::Colour (0xfff25f5c));
    addAndMakeVisible (tempoSlider);

    loopLengthLabel.setText ("Loop length", juce::dontSendNotification);
    loopLengthLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (loopLengthLabel);

    loopLengthBox.addItem ("1 bar", 1);
    loopLengthBox.addItem ("2 bars", 2);
    loopLengthBox.addItem ("4 bars", 3);
    loopLengthBox.addItem ("8 bars", 4);
    loopLengthBox.setSelectedId (3);
    addAndMakeVisible (loopLengthBox);

    countInToggle.setButtonText ("Count in");
    countInToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (countInToggle);

    metronomeToggle.setButtonText ("Metronome");
    metronomeToggle.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (metronomeToggle);

    recordButton.setButtonText ("Record");
    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xffc9342f));
    recordButton.onClick = [this]
    {
        if (isRecording)
        {
            finishRecording();
        }
        else
        {
            startRecording();
        }

        updateTransportState();
        updateTrackDisplay();
    };
    addAndMakeVisible (recordButton);

    addTrackButton.setButtonText ("+");
    addTrackButton.setTooltip ("Add track");
    addTrackButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff343b47));
    addTrackButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    addTrackButton.onClick = [this]
    {
        if (activeTrackCount >= maxTracks)
            return;

        ++activeTrackCount;

        resized();
        updateTransportState();
        updateTrackDisplay();
    };
    addAndMakeVisible (addTrackButton);

    stopButton.setButtonText ("Stop");
    stopButton.onClick = [this]
    {
        if (isRecording)
        {
            finishRecording();
        }
        else
        {
            isPlaying = false;
        }

        updateTransportState();
        updateTrackDisplay();
    };
    addAndMakeVisible (stopButton);

    statusLabel.setVisible (false);
}

void MainComponent::configureTrackRows()
{
    for (int i = 0; i < maxTracks; ++i)
    {
        auto& name = trackNameLabels[(size_t) i];
        name.setText ("Track " + juce::String (i + 1), juce::dontSendNotification);
        name.setColour (juce::Label::textColourId, juce::Colours::white);
        name.setFont (juce::Font (16.0f, juce::Font::bold));
        name.setJustificationType (juce::Justification::centred);
        name.setBorderSize ({ 0, 0, 0, 0 });
        addAndMakeVisible (name);

        auto& status = trackStatusLabels[(size_t) i];
        status.setColour (juce::Label::textColourId, juce::Colour (0xffc6cad3));
        addAndMakeVisible (status);

        auto& mute = trackMuteButtons[(size_t) i];
        mute.setButtonText ("Mute");
        mute.setClickingTogglesState (true);
        mute.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff5b6475));
        addAndMakeVisible (mute);
    }
}

void MainComponent::updateTransportState()
{
    recordButton.setButtonText (isRecording ? "Stop Rec" : "Record");
    recordButton.setEnabled (true);
    addTrackButton.setEnabled (activeTrackCount < maxTracks);
    addTrackButton.setVisible (activeTrackCount < maxTracks);
    stopButton.setEnabled (isPlaying);
}

void MainComponent::updateTrackDisplay()
{
    for (int i = 0; i < maxTracks; ++i)
    {
        const auto isActiveTrack = i < activeTrackCount;
        trackNameLabels[(size_t) i].setVisible (isActiveTrack);
        trackStatusLabels[(size_t) i].setVisible (false);
        trackMuteButtons[(size_t) i].setVisible (isActiveTrack);

        juce::String state = "Empty";

        if (isRecording && i == currentTrackIndex)
            state = "Recording current loop pass";
        else if (trackHasRecording[(size_t) i] && isPlaying)
            state = "Loop recorded and playing";
        else if (trackHasRecording[(size_t) i])
            state = "Loop recorded";

        trackStatusLabels[(size_t) i].setText (state, juce::dontSendNotification);
    }

    repaint();
}

void MainComponent::timerCallback()
{
    if (isRecording && recordingSamplePosition >= recordingLoopSamples)
    {
        trackHasRecording[(size_t) currentTrackIndex] = true;

        if (! startNextEmptyTrack())
        {
            isRecording = false;
            isPlaying = true;
            stopTimer();
        }

        updateTransportState();
        updateTrackDisplay();
    }

    repaint();
}

void MainComponent::drawWaveformTrack (juce::Graphics& g, int trackIndex)
{
    auto bounds = trackWaveformBounds[(size_t) trackIndex].toFloat();

    if (bounds.isEmpty())
        return;

    g.setColour (juce::Colour (0xff111318));
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (juce::Colour (0xff39404b));
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    auto centreY = bounds.getCentreY();
    g.setColour (juce::Colour (0xff2a3039));
    g.drawLine (bounds.getX() + 8.0f, centreY, bounds.getRight() - 8.0f, centreY, 1.0f);

    auto inner = bounds.reduced (8.0f, 7.0f);
    const auto& waveform = trackWaveforms[(size_t) trackIndex];
    const auto barWidth = inner.getWidth() / (float) waveformBinCount;
    const auto filledBins = (isRecording && trackIndex == currentTrackIndex)
                                ? juce::jlimit (0, waveformBinCount, (recordingSamplePosition * waveformBinCount) / recordingLoopSamples)
                                : (trackHasRecording[(size_t) trackIndex] ? waveformBinCount : 0);

    for (int i = 0; i < waveformBinCount; ++i)
    {
        const auto x = inner.getX() + (float) i * barWidth;
        const auto amplitude = waveform[(size_t) i];
        const auto height = juce::jmax (2.0f, amplitude * inner.getHeight());
        const auto bar = juce::Rectangle<float> (x, centreY - height * 0.5f,
                                                 juce::jmax (1.0f, barWidth - 1.0f), height);

        g.setColour (i < filledBins ? juce::Colour (0xff5ed0a5) : juce::Colour (0xff252b34));
        g.fillRoundedRectangle (bar, 1.0f);
    }

    if (isRecording && trackIndex == currentTrackIndex)
    {
        const auto progress = (float) recordingSamplePosition / (float) recordingLoopSamples;
        const auto playheadX = inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, progress);

        g.setColour (juce::Colour (0xffffd166));
        g.drawLine (playheadX, bounds.getY() + 4.0f, playheadX, bounds.getBottom() - 4.0f, 2.0f);
    }
}

void MainComponent::startRecording()
{
    if (! startNextEmptyTrack())
    {
        isRecording = false;
        isPlaying = true;
    }
}

void MainComponent::finishRecording()
{
    trackHasRecording[(size_t) currentTrackIndex] = true;
    isRecording = false;
    isPlaying = true;
    stopTimer();
    repaint();
}

bool MainComponent::startNextEmptyTrack()
{
    for (int i = 0; i < activeTrackCount; ++i)
    {
        if (! trackHasRecording[(size_t) i])
        {
            currentTrackIndex = i;
            trackWaveforms[(size_t) currentTrackIndex].fill (0.0f);
            recordingSamplePosition = 0;
            recordingLoopSamples = getLoopLengthInSamples();
            isRecording = true;
            isPlaying = true;
            startTimerHz (30);
            return true;
        }
    }

    return false;
}

int MainComponent::getSelectedLoopBars() const
{
    switch (loopLengthBox.getSelectedId())
    {
        case 1:  return 1;
        case 2:  return 2;
        case 4:  return 8;
        case 3:
        default: return 4;
    }
}

int MainComponent::getLoopLengthInSamples() const
{
    const auto beatsPerLoop = getSelectedLoopBars() * 4.0;
    const auto secondsPerLoop = beatsPerLoop * 60.0 / tempoSlider.getValue();
    return juce::jmax (1, juce::roundToInt (secondsPerLoop * currentSampleRate));
}
