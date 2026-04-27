#include "CharacterRecognitionDialog.h"
#include "ui_CharacterRecognitionDialog.h"

#include <QBrush>
#include <QButtonGroup>
#include <QColor>
#include <QFont>
#include <QGraphicsView>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QPen>
#include <QPushButton>
#include <QResizeEvent>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QComboBox>

CharacterRecognitionDialog::CharacterRecognitionDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CharacterRecognitionDialog)
    , m_segmentGroup(new QButtonGroup(this))
    , m_regionGroup(new QButtonGroup(this))
    , m_previewScene(new QGraphicsScene(this))
{
    ui->setupUi(this);
    setupUiState();
    connectControls();
}

CharacterRecognitionDialog::~CharacterRecognitionDialog()
{
    delete ui;
}

CharacterRecognitionConfig CharacterRecognitionDialog::configuration() const
{
    CharacterRecognitionConfig config;
    config.independentPositionCorrection = ui->positionCorrectionSwitch->isChecked();
    config.positionCorrection = ui->positionCorrectionComboBox->currentText();
    config.resultBasis = ui->resultBasisComboBox->currentText();
    config.minCount = ui->minCountSpinBox->value();
    config.maxCount = ui->maxCountSpinBox->value();
    config.minScore = ui->minScoreSpinBox->value();
    config.baselineText = ui->baselineTextLineEdit->text();
    config.modelName = ui->modelComboBox->currentText();
    return config;
}

QString CharacterRecognitionDialog::summaryText() const
{
    const CharacterRecognitionConfig config = configuration();

    if (config.resultBasis == tr("字符得分")) {
        return tr("%1：%2；模型：%3")
            .arg(config.resultBasis)
            .arg(config.minScore)
            .arg(config.modelName);
    }

    if (config.resultBasis == tr("基准字符")) {
        return tr("%1：%2；模型：%3")
            .arg(config.resultBasis)
            .arg(config.baselineText)
            .arg(config.modelName);
    }

    return tr("%1：%2-%3；模型：%4")
        .arg(config.resultBasis)
        .arg(config.minCount)
        .arg(config.maxCount)
        .arg(config.modelName);
}

void CharacterRecognitionDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    fitPreview();
}

void CharacterRecognitionDialog::setupUiState()
{
    setWindowTitle(tr("方案编辑 - 字符识别"));
    setMinimumSize(1440, 860);
    resize(1760, 980);
    setWindowModality(Qt::ApplicationModal);
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    ui->previewGraphicsView->setScene(m_previewScene);
    ui->previewGraphicsView->setBackgroundBrush(QColor(14, 16, 20));
    ui->previewGraphicsView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    ui->resultBasisStackedWidget->setCurrentIndex(ui->resultBasisComboBox->currentIndex());
}

void CharacterRecognitionDialog::connectControls()
{
    connect(ui->headerCloseButton, &QToolButton::clicked, this, &CharacterRecognitionDialog::reject);
    connect(ui->finishButton, &QPushButton::clicked, this, &CharacterRecognitionDialog::finishConfiguration);
    connect(ui->resultBasisComboBox,
            QOverload<int>::of(&QComboBox::currentIndexChanged),
            ui->resultBasisStackedWidget,
            &QStackedWidget::setCurrentIndex);

    m_segmentGroup->setExclusive(true);
    m_segmentGroup->addButton(ui->basicSegmentButton, 0);
    m_segmentGroup->addButton(ui->allSegmentButton, 1);

    m_regionGroup->setExclusive(true);
    m_regionGroup->addButton(ui->regionDrawButton, 0);
    m_regionGroup->addButton(ui->regionRectButton, 1);

    connect(ui->minCountSpinBox,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,[this](int value) {
        if (value > ui->maxCountSpinBox->value()) {
        ui->maxCountSpinBox->setValue(value);
        }
    });
    connect(ui->maxCountSpinBox,static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged),this,[this](int value) {
        if (value < ui->minCountSpinBox->value()) {
        ui->minCountSpinBox->setValue(value);
        }
    });
}

void CharacterRecognitionDialog::finishConfiguration()
{
    accept();
}


void CharacterRecognitionDialog::fitPreview()
{
    if (!ui->previewGraphicsView || m_previewScene->sceneRect().isEmpty()) {
        return;
    }

    ui->previewGraphicsView->fitInView(m_previewScene->sceneRect(), Qt::KeepAspectRatioByExpanding);
}
