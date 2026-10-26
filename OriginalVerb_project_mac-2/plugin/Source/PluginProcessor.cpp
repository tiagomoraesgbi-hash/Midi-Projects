#include <juce_audio_processors/juce_audio_processors.h>
#include "reverb_core.h"

class VerbProcessor : public juce::AudioProcessor {
public:
    VerbProcessor()
        : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "STATE", layout()) {}

    const juce::String getName() const override { return "OriginalVerb"; }
    void prepareToPlay(double sr, int) override { rv.prepare((float)sr); }
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
            && l.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer&) override {
        juce::ScopedNoDenormals nd;
        auto P = [&](const char* id) { return apvts.getRawParameterValue(id)->load(); };
        vv::Params p;
        p.mode = (vv::Mode)(int)P("mode");
        p.mix = P("mix") * 0.01f;       p.preDelay = P("predelay");
        p.decay = P("decay");           p.size = P("size") * 0.01f;
        p.diffusion = P("diffusion") * 0.01f;
        p.dampHz = P("damp");           p.bassMult = P("bass");
        p.modRate = P("modrate");       p.modDepth = P("moddepth") * 0.01f;
        p.erLevel = P("er") * 0.01f;
        p.lowCut = P("lowcut");         p.highCut = P("highcut");
        rv.setParams(p);
        if (b.getNumChannels() >= 2) rv.process(b.getWritePointer(0), b.getWritePointer(1), b.getNumSamples());
    }

    juce::AudioProcessorEditor* createEditor() override { return new juce::GenericAudioProcessorEditor(*this); }
    bool hasEditor() const override { return true; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 60.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& d) override {
        if (auto xml = apvts.copyState().createXml()) copyXmlToBinary(*xml, d);
    }
    void setStateInformation(const void* d, int n) override {
        if (auto xml = getXmlFromBinary(d, n)) apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout layout() {
        using F = juce::AudioParameterFloat; using R = juce::NormalisableRange<float>;
        juce::AudioProcessorValueTreeState::ParameterLayout L;
        L.add(std::make_unique<juce::AudioParameterChoice>("mode", "Mode", juce::StringArray{"Plate", "Room", "Hall"}, 2));
        L.add(std::make_unique<F>("mix", "Mix %", R(0, 100, 0.1f), 35));
        L.add(std::make_unique<F>("predelay", "PreDelay ms", R(0, 500, 0.1f, 0.4f), 0));
        L.add(std::make_unique<F>("decay", "Decay s", R(0.1f, 60, 0.01f, 0.35f), 2.5f));
        L.add(std::make_unique<F>("size", "Size %", R(0, 100, 0.1f), 50));
        L.add(std::make_unique<F>("diffusion", "Diffusion %", R(0, 100, 0.1f), 80));
        L.add(std::make_unique<F>("damp", "HiDamp Hz", R(1000, 20000, 1, 0.4f), 7000));
        L.add(std::make_unique<F>("bass", "BassMult", R(0.4f, 2.0f, 0.01f), 1.0f));
        L.add(std::make_unique<F>("modrate", "ModRate Hz", R(0.05f, 5, 0.01f, 0.5f), 0.6f));
        L.add(std::make_unique<F>("moddepth", "ModDepth %", R(0, 100, 0.1f), 40));
        L.add(std::make_unique<F>("er", "EarlyRefl %", R(0, 100, 0.1f), 50));
        L.add(std::make_unique<F>("lowcut", "LowCut Hz", R(20, 1000, 1, 0.4f), 20));
        L.add(std::make_unique<F>("highcut", "HighCut Hz", R(1000, 20000, 1, 0.4f), 20000));
        return L;
    }
    juce::AudioProcessorValueTreeState apvts;
    vv::Reverb rv;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VerbProcessor)
};

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VerbProcessor(); }
