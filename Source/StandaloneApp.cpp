#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace macro_osc
{
class NativeTitlebarStandaloneWindow final : public juce::StandaloneFilterWindow
{
public:
    NativeTitlebarStandaloneWindow (const juce::String& title,
                                    juce::Colour backgroundColour,
                                    std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::StandaloneFilterWindow (title, backgroundColour, std::move (pluginHolderIn))
    {
        setUsingNativeTitleBar (true);
        hideCustomTitlebarOptionsButton();
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        hideCustomTitlebarOptionsButton();
    }

private:
    void hideCustomTitlebarOptionsButton()
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* child = getChildComponent (i); child != nullptr && child->getName() == "Options")
                child->setVisible (false);
    }
};

class StandaloneMenuModel final : public juce::MenuBarModel
{
public:
    void setWindow (NativeTitlebarStandaloneWindow* windowToUse) noexcept
    {
        window = windowToUse;
    }

    juce::StringArray getMenuBarNames() override
    {
        return { "Options" };
    }

    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override
    {
        juce::ignoreUnused (topLevelMenuIndex, menuName);

        juce::PopupMenu menu;
        menu.addItem (1, "Audio/MIDI Settings...");
        menu.addSeparator();
        menu.addItem (2, "Save current state...");
        menu.addItem (3, "Load a saved state...");
        menu.addSeparator();
        menu.addItem (4, "Reset to default state");
        return menu;
    }

    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override
    {
        juce::ignoreUnused (topLevelMenuIndex);

        if (window != nullptr)
            window->handleMenuResult (menuItemID);
    }

private:
    NativeTitlebarStandaloneWindow* window {};
};

class NativeTitlebarStandaloneApp final : public juce::JUCEApplication
{
public:
    NativeTitlebarStandaloneApp()
    {
        juce::PropertiesFile::Options options;

        options.applicationName = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName = "~/.config";
       #else
        options.folderName = "";
       #endif

        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    NativeTitlebarStandaloneWindow* createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
        {
            jassertfalse;
            return nullptr;
        }

        return new NativeTitlebarStandaloneWindow (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
        constexpr auto autoOpenMidiDevices =
       #if (JUCE_ANDROID || JUCE_IOS) && ! JUCE_DONT_AUTO_OPEN_MIDI_DEVICES_ON_MOBILE
            true;
       #else
            false;
       #endif

       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] { JucePlugin_PreferredChannelConfigurations };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (channels,
                                                                                     juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif

        return std::make_unique<juce::StandalonePluginHolder> (appProperties.getUserSettings(),
                                                               false,
                                                               juce::String {},
                                                               nullptr,
                                                               channelConfig,
                                                               autoOpenMidiDevices);
    }

    void initialise (const juce::String&) override
    {
        mainWindow.reset (createWindow());

        if (mainWindow != nullptr)
        {
            menuModel.setWindow (mainWindow.get());
           #if JUCE_MAC
            juce::MenuBarModel::setMacMainMenu (&menuModel);
           #else
            mainWindow->setMenuBar (&menuModel);
           #endif

           #if JUCE_STANDALONE_FILTER_WINDOW_USE_KIOSK_MODE
            juce::Desktop::getInstance().setKioskModeComponent (mainWindow.get(), false);
           #endif

            mainWindow->setVisible (true);
        }
        else
        {
            pluginHolder = createPluginHolder();
        }
    }

    void shutdown() override
    {
       #if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu (nullptr);
       #else
        if (mainWindow != nullptr)
            mainWindow->setMenuBar (nullptr);
       #endif

        menuModel.setWindow (nullptr);
        pluginHolder = nullptr;
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)
            pluginHolder->savePluginState();

        if (mainWindow != nullptr)
            if (auto* holder = mainWindow->getPluginHolder())
                holder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []()
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    StandaloneMenuModel menuModel;
    std::unique_ptr<NativeTitlebarStandaloneWindow> mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};
} // namespace macro_osc

START_JUCE_APPLICATION (macro_osc::NativeTitlebarStandaloneApp)
