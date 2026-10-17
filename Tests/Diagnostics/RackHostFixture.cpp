// Test-only Rack module: rotated MIDI-CV source and Host audio recorder.
// Built and loaded into isolated Rack user folders by rack_host_timing.py.
#include <rack.hpp>
#include <cstdio>
#include <vector>

using namespace rack;

struct KeyboardFixture final : engine::Module
{
    dsp::MidiParser<16> parser;
    int64_t frame = 0;
    std::vector<float> recording;

    KeyboardFixture()
    {
        config (0, 1, 2);
        parser.channels = 8;
        recording.reserve (589824);
    }

    void process (const ProcessArgs& args) override
    {
        static constexpr int notes[] { 48, 50, 52, 55, 57, 60, 62, 64,
                                      67, 69, 72, 74, 76, 79, 81, 84 };
        constexpr int start = 65536, period = 32768, hold = 2048;
        constexpr int end = start + 16 * period;
        if (frame >= start && frame < end)
        {
            const int position = static_cast<int> (frame - start);
            const int time = position % period;
            if (time == 0 || time == hold)
            {
                midi::Message message;
                message.setStatus (time == 0 ? 0x9 : 0x8);
                message.setNote (notes[position / period]);
                message.setValue (time == 0 ? 100 : 0);
                parser.processMessage (message);
            }
        }
        parser.processFilters (args.sampleTime);
        for (auto& output : outputs) output.setChannels (8);
        for (int channel = 0; channel < 8; ++channel)
        {
            outputs[0].setVoltage (parser.getPitchVoltage (channel), channel);
            outputs[1].setVoltage (parser.gates[channel] ? 10.0f : 0.0f, channel);
        }
        parser.processPulses (args.sampleTime);
        if (frame < end) recording.push_back (inputs[0].getVoltage());
        if (frame == end)
        {
            // Offline diagnostic only: file writes are deliberately outside
            // production DSP. The runner waits for the JSON completion marker.
            if (auto* file = std::fopen (asset::user ("audio.f32").c_str(), "wb"))
            {
                std::fwrite (recording.data(), sizeof (float), recording.size(), file);
                std::fclose (file);
            }
            if (auto* host = APP->engine->getModule (4))
            {
                auto* state = host->dataToJson();
                json_dump_file (state, asset::user ("host-result.json").c_str(), JSON_INDENT (2));
                json_decref (state);
            }
        }
        ++frame;
    }
};

struct KeyboardFixtureWidget final : app::ModuleWidget
{
    explicit KeyboardFixtureWidget (KeyboardFixture* module)
    {
        setModule (module);
        box.size = math::Vec (45, 380);
        // Rack's patch loader requires port widgets even in headless mode.
        addInput (createInput<PJ301MPort> (math::Vec (10, 40), module, 0));
        addOutput (createOutput<PJ301MPort> (math::Vec (10, 80), module, 0));
        addOutput (createOutput<PJ301MPort> (math::Vec (10, 120), module, 1));
    }
};

extern "C" void init (plugin::Plugin* plugin)
{
    plugin->addModel (createModel<KeyboardFixture, KeyboardFixtureWidget> ("KeyboardFixture"));
}
