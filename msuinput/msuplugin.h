#pragma once

#include <core/engine/inputplugin.h>
#include <core/plugins/plugin.h>
#include <QObject>
#include "msuinput.h"

namespace Fooyin::MSU {

    class MSUPlugin : public QObject, public Plugin, public InputPlugin
    {
        Q_OBJECT
        Q_PLUGIN_METADATA(IID "org.fooyin.fooyin.plugin/1.0" FILE "msuinput.json")
        Q_INTERFACES(Fooyin::Plugin Fooyin::InputPlugin)
        
    public:
        [[nodiscard]] QString inputName() const override;
        [[nodiscard]] InputCreator inputCreator() const override;
    };
    
} // namespace Fooyin::MSU
