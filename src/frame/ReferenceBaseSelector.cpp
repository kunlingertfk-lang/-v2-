#include "frame/ReferenceBaseSelector.h"

#include "frame/ReferenceImageProvider.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

ReferenceBaseSelector::ReferenceBaseSelector(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("referenceBaseSelector"));
    setProperty("role", QStringLiteral("referenceBaseSelector"));

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_label = new QLabel(tr("验证基准图"), this);
    m_label->setObjectName(QStringLiteral("referenceBaseSelectorLabel"));
    m_comboBox = new QComboBox(this);
    m_comboBox->setObjectName(QStringLiteral("referenceBaseComboBox"));
    m_comboBox->setMinimumContentsLength(12);
    m_validateAllButton = new QPushButton(tr("验证全部"), this);
    m_validateAllButton->setObjectName(
                QStringLiteral("validateAllReferenceBasesButton"));

    layout->addWidget(m_label);
    layout->addWidget(m_comboBox, 1);
    layout->addWidget(m_validateAllButton);

    connect(m_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0)
            emit selectedBaseChanged(selectedBaseId());
    });
    connect(m_validateAllButton, &QPushButton::clicked,
            this, &ReferenceBaseSelector::validateAllRequested);
    connect(&ReferenceImageProvider::instance(),
            &ReferenceImageProvider::referenceFrameSetChanged,
            this, &ReferenceBaseSelector::refresh);

    refresh();
}

QString ReferenceBaseSelector::selectedBaseId() const
{
    return m_comboBox
            ? m_comboBox->currentData(Qt::UserRole).toString().trimmed()
            : QString();
}

void ReferenceBaseSelector::setSelectedBaseId(const QString &baseId)
{
    if (!m_comboBox)
        return;
    const QString wanted = baseId.trimmed();
    for (int index = 0; index < m_comboBox->count(); ++index) {
        if (m_comboBox->itemData(index, Qt::UserRole).toString() != wanted)
            continue;
        m_comboBox->setCurrentIndex(index);
        return;
    }
}

void ReferenceBaseSelector::setValidateAllVisible(bool visible)
{
    if (m_validateAllButton)
        m_validateAllButton->setVisible(visible);
}

void ReferenceBaseSelector::refresh()
{
    if (!m_comboBox)
        return;
    QString previous = selectedBaseId();
    const QList<ReferenceFrameEntry> entries =
            ReferenceImageProvider::instance().referenceFrames();
    const QSignalBlocker blocker(m_comboBox);
    m_comboBox->clear();
    for (int index = 0; index < entries.size(); ++index) {
        const ReferenceFrameEntry &entry = entries.at(index);
        const QString name = entry.name.trimmed().isEmpty()
                ? tr("Base %1").arg(index + 1) : entry.name.trimmed();
        m_comboBox->addItem(
                    tr("B%1 · %2").arg(index + 1, 2, 10, QLatin1Char('0'))
                    .arg(name),
                    entry.baseId);
    }
    if (m_comboBox->count() == 0) {
        m_comboBox->addItem(tr("暂无基准图"), QString());
        m_comboBox->setEnabled(false);
        if (m_validateAllButton)
            m_validateAllButton->setEnabled(false);
        return;
    }
    m_comboBox->setEnabled(true);
    if (m_validateAllButton)
        m_validateAllButton->setEnabled(true);
    if (previous.isEmpty())
        previous = ReferenceImageProvider::instance().primaryBaseId();
    for (int index = 0; index < m_comboBox->count(); ++index) {
        if (m_comboBox->itemData(index, Qt::UserRole).toString() == previous) {
            m_comboBox->setCurrentIndex(index);
            return;
        }
    }
    m_comboBox->setCurrentIndex(0);
}
