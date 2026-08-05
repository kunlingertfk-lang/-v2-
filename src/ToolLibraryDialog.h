#ifndef TOOLLIBRARYDIALOG_H
#define TOOLLIBRARYDIALOG_H

#include <QDialog>

#include "toolcore/ToolTypes.h"

class QButtonGroup;

QT_BEGIN_NAMESPACE
namespace Ui {
class ToolLibraryDialog;
}
QT_END_NAMESPACE

class ToolLibraryDialog : public QDialog
{
    Q_OBJECT

public:
    enum ToolId {
        NoTool = -1,
        Presence = 0,
        Counter,
        Judge,
        Category,
        ColorArea,
        CharacterRecognition,   //字符识别
        Code,
        BlobPresence,
        CirclePresence,
        EdgePresence,
        LinePresence,
        ContourPresence,
        //AI检测 
        ObjectDetection,        //目标检测
        Classification,         //分类检测  resnet18

/*===========================tfk add===========================*/
        //定位工具
        TemplateLocation,       //模板定位
        EdgeLocationButton,     //边缘定位
        CircleLocationButton,   //圆定位
        PositionCorrectionTool,
        CalibrationTransformTool,

        //识别工具
        ColorRecognition,       //颜色识别
        ColorComparison,        //颜色比较
        RegistrationClass,      //注册分类
        RegistrationClassDetection,      //注册目标检测
        RegisteredObjectDetection        //注册目标检测
/*===========================tfk end===========================*/




    };

    explicit ToolLibraryDialog(QWidget *parent = nullptr);
    ~ToolLibraryDialog() override;

    ToolId selectedTool() const;
    ToolType selectedToolType() const;

private slots:
    void confirmSelection();
    void updatePreview(int id);

private:
    void setupUiState();
    void setupButtonGroup();

    Ui::ToolLibraryDialog *ui;
    QButtonGroup *m_buttonGroup;
    ToolType m_selectedToolType = ToolType::Unknown;
};

#endif // TOOLLIBRARYDIALOG_H
