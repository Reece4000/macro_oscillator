// Offline diagnostic for a pitch path that is one cable longer than gate.
// Uses Rack's engine/MIDI parser and the installed Quantum and VCA modules.
// See RackCvTiming.md. This is not part of the normal JUCE test build.
#include <rack.hpp>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace rack;

namespace
{
struct Keyboard final : engine::Module
{
    dsp::MidiParser<16> parser;
    explicit Keyboard (int channels)
    {
        config (0, 0, 2);
        parser.channels = channels;
    }
    void note (int pitch, bool on)
    {
        midi::Message message;
        message.setStatus (on ? 0x9 : 0x8);
        message.setNote (pitch);
        message.setValue (on ? 100 : 0);
        parser.processMessage (message);
    }
    void process (const ProcessArgs& args) override
    {
        parser.processFilters (args.sampleTime);
        for (auto& output : outputs) output.setChannels (parser.channels);
        for (int channel = 0; channel < parser.channels; ++channel)
        {
            outputs[0].setVoltage (parser.getPitchVoltage (channel), channel);
            outputs[1].setVoltage (parser.gates[channel] ? 10.0f : 0.0f, channel);
        }
        parser.processPulses (args.sampleTime);
    }
};

// Measures pitch at the gate edge, the point where VCV Host documents that it
// creates a MIDI note. Does not emulate Host's proprietary VST implementation.
struct GatePitchProbe final : engine::Module
{
    bool gates[16] {};
    std::vector<int> latched;
    GatePitchProbe() { config (0, 2, 0); }
    void process (const ProcessArgs&) override
    {
        for (int channel = 0; channel < inputs[1].getChannels(); ++channel)
        {
            const bool high = inputs[1].getVoltage (channel) >= 1.0f;
            if (high && ! gates[channel])
                latched.push_back (std::lround (inputs[0].getPolyVoltage (channel) * 12.0f) + 60);
            gates[channel] = high;
        }
    }
};

plugin::Plugin* loadPlugin (const std::string& userDirectory, const std::string& slug)
{
    auto* plugin = new plugin::Plugin;
    plugin->path = userDirectory + "/plugins-mac-arm64/" + slug;
    plugin->handle = dlopen ((plugin->path + "/plugin.dylib").c_str(), RTLD_NOW | RTLD_LOCAL);
    if (plugin->handle == nullptr) throw std::runtime_error (dlerror());
    const auto initialise = reinterpret_cast<void (*) (plugin::Plugin*)> (dlsym (plugin->handle, "init"));
    if (initialise == nullptr) throw std::runtime_error (dlerror());
    initialise (plugin);
    return plugin;
}

void connect (engine::Engine& engine, engine::Module* from, int output, engine::Module* to, int input)
{
    auto* cable = new engine::Cable;
    cable->outputModule = from;
    cable->outputId = output;
    cable->inputModule = to;
    cable->inputId = input;
    engine.addCable (cable);
}
}

int main (int argc, char** argv)
{
    int result = 2;
    try
    {
        if (argc < 3) throw std::runtime_error ("usage: RackCvTiming <Rack user folder> original|aligned|direct [channels]");
        const std::string mode = argv[2];
        if (mode != "original" && mode != "aligned" && mode != "direct")
            throw std::runtime_error ("unknown routing mode");
        const int channels = argc > 3 ? std::stoi (argv[3]) : 8;
        if (channels < 1 || channels > 16) throw std::runtime_error ("channels must be 1..16");
        random::init();
        Context context;
        contextSet (&context);
        context.engine = new engine::Engine;
        auto& engine = *context.engine;
        engine.setSampleRate (48000.0f);
        auto* ml = loadPlugin (argv[1], "ML_modules");
        auto* fundamental = loadPlugin (argv[1], "Fundamental");
        auto* quantum = ml->getModel ("Quantum")->createModule();
        json_error_t error;
        auto* data = json_loads ("{\"scale\":[1,0,1,0,1,0,0,1,0,1,0,0],\"mode\":1,\"transpose_select\":1,\"toggle_mode\":0}", 0, &error);
        quantum->dataFromJson (data);
        json_decref (data);
        auto* keyboard = new Keyboard (channels);
        auto* vca = fundamental->getModel ("VCA-1")->createModule();
        auto* probe = new GatePitchProbe;
        engine.setMasterModule (keyboard);
        for (auto* module : std::vector<engine::Module*> { keyboard, quantum, vca, probe })
            engine.addModule (module);
        if (mode == "direct") connect (engine, keyboard, 0, probe, 0);
        else
        {
            connect (engine, keyboard, 0, quantum, 0);
            connect (engine, quantum, 0, probe, 0);
        }
        if (mode == "aligned")
        {
            connect (engine, keyboard, 1, vca, 1);
            connect (engine, vca, 0, probe, 1);
        }
        else connect (engine, keyboard, 1, probe, 1);

        engine.stepBlock (16);
        const std::vector<int> notes { 48, 50, 52, 55, 57, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84 };
        for (int pitch : notes)
        {
            const auto before = probe->latched.size();
            keyboard->note (pitch, true);
            engine.stepBlock (16);
            std::cout << "played=" << pitch << " gate-edge pitch="
                      << (probe->latched.size() > before ? probe->latched.back() : -1) << '\n';
            keyboard->note (pitch, false);
            engine.stepBlock (16);
        }
        result = probe->latched == notes ? 0 : 1;
        std::cout << (result == 0 ? "PASS: " : "FAIL: ") << mode << '\n';
        engine.clear();
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; }
    return result;
}
