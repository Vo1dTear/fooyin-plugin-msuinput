#include "msuplugin.h"
#include "msusettingswidget.h"
#include "msuinput.h"

#include <core/coresettings.h>
#include <gui/plugins/pluginsettingsprovider.h>

namespace Fooyin::MSU {
    
    namespace {
        
        class MSUSettingsProvider final : public PluginSettingsProvider
        {
        public:
            QDialog* createSettings(QWidget* parent) override
            {
                return new MSUSettingsWidget(parent);
            }
        };
        
    } // namespace
    
    QString MSUPlugin::inputName() const
    {
        return QStringLiteral("MSU-1");
    }
    
    InputCreator MSUPlugin::inputCreator() const
    {
        InputCreator creator;
        creator.decoder = []() {
            auto decoder = std::make_unique<MSUDecoder>();
            
            Fooyin::FySettings settings;
            double gainDb = settings.value("MSU-1/Gain", 0.0).toDouble();
            quint32 loopCount = settings.value("MSU-1/LoopCount", 0).toUInt();
            bool enableLoop = settings.value("MSU-1/EnableLoop", true).toBool();
            
            decoder->setGain(gainDb);
            decoder->setLoopCount(loopCount);
            decoder->setEnableLoop(enableLoop);
            
            return decoder;
        };
        creator.reader = []() { return std::make_unique<MSUReader>(); };
        return creator;
    }
    
    std::unique_ptr<PluginSettingsProvider> MSUPlugin::settingsProvider() const
    {
        return std::make_unique<MSUSettingsProvider>();
    }
    
} // namespace Fooyin::MSU

#include "moc_msuplugin.cpp"
