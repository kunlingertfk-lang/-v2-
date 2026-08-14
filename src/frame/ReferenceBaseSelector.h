#ifndef FRAME_REFERENCEBASESELECTOR_H
#define FRAME_REFERENCEBASESELECTOR_H

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;

/**
 * Per-window selector for a scheme reference Base.
 *
 * The selector never changes ReferenceImageProvider::primaryBaseId().  This is
 * deliberate: two open tool editors can validate against different Base
 * images without changing one another or the live tool-chain state.
 */
class ReferenceBaseSelector : public QWidget
{
    Q_OBJECT

public:
    explicit ReferenceBaseSelector(QWidget *parent = nullptr);

    QString selectedBaseId() const;
    void setSelectedBaseId(const QString &baseId);
    void setValidateAllVisible(bool visible);
    void refresh();

signals:
    void selectedBaseChanged(const QString &baseId);
    void validateAllRequested();

private:
    QComboBox *m_comboBox = nullptr;
    QLabel *m_label = nullptr;
    QPushButton *m_validateAllButton = nullptr;
};

#endif // FRAME_REFERENCEBASESELECTOR_H
