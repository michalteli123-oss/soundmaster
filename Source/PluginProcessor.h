#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

class SoundMasterAudioProcessor : public juce::AudioProcessor
{
public:
    SoundMasterAudioProcessor();
    ~SoundMasterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "SoundMaster"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateMacro (float a);

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    // EQ + de-esser (po jednym filtrze na kanal)
    Filter hpf[2], mud[2], presence[2], air[2], deEssDetect[2];

    juce::dsp::Compressor<float> compressor;
    juce::dsp::Limiter<float> limiter;

    // pogłos (wysyłka)
    juce::Reverb reverb;
    juce::dsp::DelayLine<float> preDelay { 1 };
    juce::dsp::StateVariableTPTFilter<float> reverbHp, reverbLp;
    juce::AudioBuffer<float> wetBuffer;

    juce::SmoothedValue<float> macroSmoothed;
    std::atomic<float>* pMacro = nullptr;

    double sr = 44100.0;
    float lastApplied = -1.0f;

    // wartości wyliczane z pokrętła
    float deEssAmount = 0.0f, satMix = 0.0f, satDrive = 1.0f;
    float makeupGain = 1.0f, lastMakeupGain = 1.0f;
    float reverbGain = 0.0f, lastReverbGain = 0.0f;

    // detektor sybilantów
    float envFull = 0.0f, envHf = 0.0f, envAttack = 0.0f, envRelease = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SoundMasterAudioProcessor)
};
