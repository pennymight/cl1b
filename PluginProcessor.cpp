#include "PluginProcessor.h"

using APVTS = juce::AudioProcessorValueTreeState;
using Range = juce::NormalisableRange<float>;

APVTS::ParameterLayout OptoCompAudioProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto add = [&] (const char* id, const char* name, Range r, float def, const char* unit)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, r, def,
            juce::AudioParameterFloatAttributes().withLabel (unit)));
    };

    add ("input",     "Input",     { -12.f, 24.f, 0.1f },        0.f,   "dB");
    add ("threshold", "Threshold", { -50.f, 0.f, 0.1f },       -18.f,  "dB");
    add ("ratio",     "Ratio",     { 1.5f, 10.f, 0.1f, 0.6f },  3.f,   ":1");
    add ("attack",    "Attack",    { 0.2f, 30.f, 0.1f, 0.5f },  8.f,   "ms");
    add ("release",   "Release",   { 50.f, 5000.f, 1.f, 0.4f }, 800.f, "ms");
    add ("drive",     "Tube Drive",{ 0.f, 1.f, 0.01f },         0.2f,  "");
    add ("makeup",    "Makeup",    { -12.f, 24.f, 0.1f },        0.f,   "dB");
    add ("mix",       "Mix",       { 0.f, 1.f, 0.01f },          1.f,   "");

    return { p.begin(), p.end() };
}

OptoCompAudioProcessor::OptoCompAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

void OptoCompAudioProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    gr = grAvg = 0.f;
    for (int i = 0; i < 2; ++i) dcX[i] = dcY[i] = 0.f;
}

bool OptoCompAudioProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    return l.getMainInputChannelSet() == l.getMainOutputChannelSet()
        && (l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
         || l.getMainOutputChannelSet() == juce::AudioChannelSet::mono());
}

void OptoCompAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n   = buffer.getNumSamples();

    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    const float inGain   = juce::Decibels::decibelsToGain (get ("input"));
    const float thresh   = get ("threshold");
    const float ratio    = get ("ratio");
    const float atkMs    = get ("attack");
    const float relMaxMs = get ("release");
    const float drive    = get ("drive");
    const float makeup   = get ("makeup");
    const float mix      = get ("mix");

    const float slope    = 1.0f - 1.0f / ratio;
    const float knee     = 6.0f;
    const float atkCoef  = std::exp (-1.0f / (0.001f * atkMs * (float) sr));
    const float avgCoef  = std::exp (-1.0f / (2.0f * (float) sr)); // ~2 s program memory

    // tube stage
    const float k    = 1.0f + 4.0f * drive;
    const float bias = 0.15f * drive;
    const float tb   = std::tanh (bias);
    const float norm = 1.0f / (k * (1.0f - tb * tb));

    for (int i = 0; i < n; ++i)
    {
        // linked stereo detector
        float peak = 0.f;
        for (int c = 0; c < nCh; ++c)
            peak = juce::jmax (peak, std::abs (buffer.getReadPointer (c)[i] * inGain));

        const float levelDb = juce::Decibels::gainToDecibels (peak, -100.0f);
        const float over = levelDb - thresh;

        // soft-knee gain computer
        float target;
        if (2.f * over < -knee)              target = 0.f;
        else if (2.f * std::abs (over) <= knee)
        {
            const float t = over + knee * 0.5f;
            target = slope * t * t / (2.f * knee);
        }
        else                                 target = slope * over;

        // opto-style program-dependent release:
        // little recent reduction -> fast recovery, sustained reduction -> slow recovery
        float coef = atkCoef;
        if (target < gr)
        {
            const float w = juce::jlimit (0.f, 1.f, grAvg / 10.f);
            const float relMs = relMaxMs * (0.15f + 0.85f * w);
            coef = std::exp (-1.0f / (0.001f * relMs * (float) sr));
        }
        gr = target + coef * (gr - target);
        grAvg = gr + avgCoef * (grAvg - gr);

        const float gain = juce::Decibels::decibelsToGain (-gr + makeup);

        for (int c = 0; c < nCh; ++c)
        {
            auto* d = buffer.getWritePointer (c);
            const float dry = d[i];
            float wet = dry * inGain * gain;

            if (drive > 0.001f)
            {
                float s = (std::tanh (k * wet + bias) - tb) * norm;
                // remove DC created by the asymmetric (even-harmonic) curve
                dcY[c] = s - dcX[c] + 0.995f * dcY[c];
                dcX[c] = s;
                wet = dcY[c];
            }
            d[i] = dry * (1.f - mix) + wet * mix;
        }
    }
}

juce::AudioProcessorEditor* OptoCompAudioProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void OptoCompAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void OptoCompAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OptoCompAudioProcessor();
}
