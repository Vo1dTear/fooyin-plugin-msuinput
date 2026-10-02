#include "msusettingswidget.h"
#include <gui/widgets/doubleslidereditor.h>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

using namespace Qt::StringLiterals;

namespace Fooyin::MSU {

    MSUSettingsWidget::MSUSettingsWidget(QWidget* parent)
    : QDialog(parent)
    , m_loopCount(new QSpinBox(this))
    , m_gain(new DoubleSliderEditor(tr("Gain"), this))
    {
        setWindowTitle(tr("MSU-1 Settings"));
        setModal(true);

        auto* buttons = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, this);
        connect(buttons, &QDialogButtonBox::accepted, this, &MSUSettingsWidget::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &MSUSettingsWidget::reject);
        connect(buttons->button(QDialogButtonBox::Reset), &QAbstractButton::clicked,
                this, &MSUSettingsWidget::reset);

        // Gain configuration
        m_gain->setRange(-12, 12);
        m_gain->setSuffix(u" dB"_s);

        // Loop count configuration
        auto* loopRow = new QWidget(this);
        auto* loopRowLayout = new QHBoxLayout(loopRow);
        loopRowLayout->setContentsMargins(0, 0, 0, 0);
        loopRowLayout->setSpacing(6);

        m_loopCount->setRange(0, 16);
        m_loopCount->setSingleStep(1);
        m_loopCount->setSuffix(u" "_s + tr("times"));

        auto* loopHintLabel = new QLabel(tr("(0 = infinite)"), this);
        loopHintLabel->setStyleSheet(u"color: gray; font-style: italic;"_s);

        loopRowLayout->addWidget(m_loopCount);
        loopRowLayout->addWidget(loopHintLabel);
        loopRowLayout->addStretch();

        // Main layout
        auto* layout = new QFormLayout(this);
        layout->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        layout->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        layout->setSizeConstraint(QLayout::SetFixedSize);

        layout->addRow(m_gain);  // DoubleSliderEditor already has its own label

        m_enableLoop = new QCheckBox(tr("Enable Loop"), this);
        layout->addRow(m_enableLoop); // <- added before Loop count
        layout->addRow(tr("Loop count"), loopRow);

        // Visual separator
        auto* separator = new QFrame(this);
        separator->setFrameShape(QFrame::HLine);
        separator->setFrameShadow(QFrame::Sunken);
        layout->addRow(separator);

        layout->addRow(buttons);

        // Load values from FySettings
        loadSettings();
    }

    void MSUSettingsWidget::accept()
    {
        m_settings.setValue("MSU-1/Gain", m_gain->value());
        m_settings.setValue("MSU-1/LoopCount", m_loopCount->value());
        m_settings.setValue("MSU-1/EnableLoop", m_enableLoop->isChecked());
        done(Accepted);
    }

    void MSUSettingsWidget::reset()
    {
        m_gain->setValue(0.0);
        m_loopCount->setValue(0);
        m_enableLoop->setChecked(true);
    }

    void MSUSettingsWidget::loadSettings()
    {
        m_gain->setValue(m_settings.value("MSU-1/Gain", 0.0).toDouble());
        m_loopCount->setValue(m_settings.value("MSU-1/LoopCount", 0).toInt());
        m_enableLoop->setChecked(m_settings.value("MSU-1/EnableLoop", true).toBool());
    }

} // namespace Fooyin::MSU
