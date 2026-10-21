#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

using APVTS = juce::AudioProcessorValueTreeState;

APVTS::ParameterLayout SoundMasterAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    p.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "soundmaster", 1 },
        "Soundmaster",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    return { p.begin(), p.end() };
}

SoundMasterAudioProcessor::SoundMasterAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    pMacro = apvts.getRawParameterValue ("soundmaster");
}

bool SoundMasterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void SoundMasterAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;

    juce::dsp::ProcessSpec spec { sampleRate,
                                  (juce::uint32) samplesPerBlock,
                                  (juce::uint32) getTotalNumOutputChannels() };

    compressor.prepare (spec);
    compressor.setAttack (8.0f);
    compressor.setRelease (140.0f);

    limiter.prepare (spec);
    limiter.setThreshold (-0.5f);
    limiter.setRelease (60.0f);

    preDelay.setMaximumDelayInSamples ((int) (sampleRate * 0.1) + 1);
    preDelay.prepare (spec);
    preDelay.setDelay ((float) (0.025 * sampleRate));
    preDelay.reset();

    reverbHp.prepare (spec);
    reverbHp.setType (juce::dsp::StateVariableTPTFilterType::highpass);
    reverbHp.setCutoffFrequency (250.0f);
    reverbHp.reset();

    reverbLp.prepare (spec);
    reverbLp.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    reverbLp.setCutoffFrequency (9000.0f);
    reverbLp.reset();

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    wetBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()), samplesPerBlock);

    // detektor sybilantów: górnoprzepustowy 6 kHz
    auto detCoeffs = Coeffs::makeHighPass (sampleRate, 6000.0f, 0.707f);
    for (int ch = 0; ch < 2; ++ch)
    {
        deEssDetect[ch].coefficients = detCoeffs;
        deEssDetect[ch].reset();
        hpf[ch].reset();
        mud[ch].reset();
        presence[ch].reset();
        air[ch].reset();
    }

    envFull = envHf = 0.0f;
    envAttack  = std::exp (-1.0f / (0.001f * (float) sampleRate));
    envRelease = std::exp (-1.0f / (0.030f * (float) sampleRate));

    const float a = pMacro->load() * 0.01f;
    macroSmoothed.reset (sampleRate, 0.05);
    macroSmoothed.setCurrentAndTargetValue (a);

    updateMacro (a);
    lastApplied = a;
    lastMakeupGain = makeupGain;
    lastReverbGain = reverbGain;
}

// Tu dzieje się cała "magia": jedno pokrętło 0..1 ustawia wszystkie moduły.
void SoundMasterAudioProcessor::updateMacro (float a)
{
    // --- EQ ---
    const float hpFreq = 20.0f + 65.0f * a;                                   // czyszczenie dołu 20 -> 85 Hz
    auto hpCoeffs   = Coeffs::makeHighPass (sr, hpFreq, 0.707f);
    auto mudCoeffs  = Coeffs::makePeakFilter (sr, 300.0f, 1.0f,
                          juce::Decibels::decibelsToGain (-3.0f * a));       // wycięcie "pudła" do -3 dB
    auto presCoeffs = Coeffs::makePeakFilter (sr, 3200.0f, 0.8f,
                          juce::Decibels::decibelsToGain (3.0f * std::sqrt (a))); // obecność do +3 dB
    auto airCoeffs  = Coeffs::makeHighShelf (sr, 10000.0f, 0.7f,
                          juce::Decibels::decibelsToGain (4.0f * a));        // "powietrze" do +4 dB

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[ch].coefficients = hpCoeffs;
        mud[ch].coefficients = mudCoeffs;
        presence[ch].coefficients = presCoeffs;
        air[ch].coefficients = airCoeffs;
    }

    // --- de-esser ---
    deEssAmount = 0.75f * a;

    // --- kompresor ---
    const float thr = -6.0f - 18.0f * a;     // próg -6 -> -24 dB
    const float ratio = 1.0f + 3.5f * a;     // stosunek 1:1 -> 4.5:1
    compressor.setThreshold (thr);
    compressor.setRatio (ratio);
    makeupGain = juce::Decibels::decibelsToGain (0.4f * (-thr) * (1.0f - 1.0f / ratio));

    // --- saturacja (ciepło) ---
    satMix = 0.7f * a;
    satDrive = 1.0f + 1.5f * a;

    // --- pogłos ---
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.35f + 0.30f * a;
    rp.damping    = 0.6f;
    rp.width      = 1.0f;
    rp.wetLevel   = 1.0f;
    rp.dryLevel   = 0.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);
    reverbGain = 0.24f * std::pow (a, 1.3f);
}

void SoundMasterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, n);

    // płynna zmiana pokrętła
    macroSmoothed.setTargetValue (pMacro->load() * 0.01f);
    const float a = macroSmoothed.skip (n);
    if (lastApplied < 0.0f || std::abs (a - lastApplied) > 0.0005f)
    {
        updateMacro (a);
        lastApplied = a;
    }

    float* data[2] = { buffer.getWritePointer (0),
                       numCh > 1 ? buffer.getWritePointer (1) : nullptr };

    // 1) HPF -> de-esser -> EQ
    for (int i = 0; i < n; ++i)
    {
        float x[2] = { 0.0f, 0.0f };
        float hf[2] = { 0.0f, 0.0f };
        float fullAbs = 0.0f, hfAbs = 0.0f;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float s = hpf[ch].processSample (data[ch][i]);
            const float h = deEssDetect[ch].processSample (s);
            x[ch] = s;
            hf[ch] = h;
            fullAbs = juce::jmax (fullAbs, std::abs (s));
            hfAbs = juce::jmax (hfAbs, std::abs (h));
        }

        envFull = fullAbs + (fullAbs > envFull ? envAttack : envRelease) * (envFull - fullAbs);
        envHf   = hfAbs   + (hfAbs   > envHf   ? envAttack : envRelease) * (envHf   - hfAbs);

        // sybilant = dominuje górne pasmo względem całości (niezależne od głośności)
        const float ratio = envHf / (envFull + 1.0e-6f);
        float reduction = juce::jlimit (0.0f, 1.0f, (ratio - 0.35f) / 0.4f) * deEssAmount;
        reduction *= juce::jlimit (0.0f, 1.0f, (envFull - 0.002f) / 0.01f); // cisza = bez reakcji

        for (int ch = 0; ch < numCh; ++ch)
        {
            float s = x[ch] - hf[ch] * reduction;
            s = mud[ch].processSample (s);
            s = presence[ch].processSample (s);
            s = air[ch].processSample (s);
            data[ch][i] = s;
        }
    }

    // 2) kompresor
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        compressor.process (ctx);
    }

    // 3) saturacja + wyrównanie głośności
    for (int ch = 0; ch < numCh; ++ch)
    {
        for (int i = 0; i < n; ++i)
        {
            const float s = data[ch][i];
            const float sat = std::tanh (satDrive * s) / satDrive;
            data[ch][i] = s + satMix * (sat - s);
        }
    }
    buffer.applyGainRamp (0, n, lastMakeupGain, makeupGain);
    lastMakeupGain = makeupGain;

    // 4) pogłos (wysyłka równoległa)
    wetBuffer.setSize (numCh, n, false, false, true);
    for (int ch = 0; ch < numCh; ++ch)
        wetBuffer.copyFrom (ch, 0, buffer, ch, 0, n);

    {
        juce::dsp::AudioBlock<float> wb (wetBuffer);
        juce::dsp::ProcessContextReplacing<float> wctx (wb);
        preDelay.process (wctx);
        reverbHp.process (wctx);
        reverbLp.process (wctx);
    }

    if (numCh >= 2)
        reverb.processStereo (wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1), n);
    else
        reverb.processMono (wetBuffer.getWritePointer (0), n);

    for (int ch = 0; ch < numCh; ++ch)
        buffer.addFromWithRamp (ch, 0, wetBuffer.getReadPointer (ch), n, lastReverbGain, reverbGain);
    lastReverbGain = reverbGain;

    // 5) limiter bezpieczeństwa
    {
        juce::dsp::AudioBlock<float> ob (buffer);
        juce::dsp::ProcessContextReplacing<float> octx (ob);
        limiter.process (octx);
    }
}

juce::AudioProcessorEditor* SoundMasterAudioProcessor::createEditor()
{
    return new SoundMasterAudioProcessorEditor (*this);
}

void SoundMasterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SoundMasterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SoundMasterAudioProcessor();
}
