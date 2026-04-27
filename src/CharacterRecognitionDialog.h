#ifndef CHARACTERRECOGNITIONDIALOG_H
#define CHARACTERRECOGNITIONDIALOG_H

#include <QDialog>

class QButtonGroup;
class QGraphicsScene;
class QResizeEvent;

QT_BEGIN_NAMESPACE
namespace Ui {
class CharacterRecognitionDialog;
}
QT_END_NAMESPACE

struct CharacterRecognitionConfig
{
    bool independentPositionCorrection = true;
    QString positionCorrection;
    QString resultBasis;
    int minCount = 1;
    int maxCount = 10;
    int minScore = 50;
    QString baselineText;
    QString modelName;
};

class CharacterRecognitionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CharacterRecognitionDialog(QWidget *parent = nullptr);
    ~CharacterRecognitionDialog() override;

    CharacterRecognitionConfig configuration() const;
    QString summaryText() const;

protected:
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void finishConfiguration();

private:
    void setupUiState();
    void connectControls();
    void fitPreview();

    Ui::CharacterRecognitionDialog *ui;
    QButtonGroup *m_segmentGroup;
    QButtonGroup *m_regionGroup;
    QGraphicsScene *m_previewScene;
};

#endif // CHARACTERRECOGNITIONDIALOG_H
