#pragma once

#include <QDialog>
#include <core/coresettings.h>
#include <QCheckBox>
#include <gui/widgets/doubleslidereditor.h>

class QSpinBox;

namespace Fooyin::MSU {

    class MSUSettingsWidget : public QDialog
    {
        Q_OBJECT

    public:
        explicit MSUSettingsWidget(QWidget* parent = nullptr);

    private:
        QCheckBox* m_enableLoop{nullptr};
        QSpinBox* m_loopCount{nullptr};
        DoubleSliderEditor* m_gain{nullptr};

        Fooyin::FySettings m_settings;

        void accept() override;  // <-- override declaration
        void loadSettings();
        void reset();
    };

} // namespace Fooyin::MSU
