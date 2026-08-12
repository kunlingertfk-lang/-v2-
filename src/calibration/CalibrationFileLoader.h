#ifndef CALIBRATION_CALIBRATIONFILELOADER_H
#define CALIBRATION_CALIBRATIONFILELOADER_H

#include "calibration/CalibrationModel.h"

#include <QByteArray>
#include <QString>

class CalibrationFileLoader
{
public:
    virtual ~CalibrationFileLoader() = default;
    /// 返回用于结果诊断的稳定格式标识。
    virtual QString formatId() const = 0;
    /// 仅探测扩展名及文件根节点，不承诺文件已通过完整校验。
    virtual bool canLoad(const QString &filePath) const = 0;
    /// 读取并完整校验标定模型；失败时不得输出可执行模型。
    virtual bool load(const QString &filePath,
                      CalibrationModel *model,
                      QString *errorMessage = nullptr) const = 0;
};

/// 项目自有 Calibration XML 1.4 的读写器，包含结构、校验和与派生模型一致性校验。
class ProjectXmlCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override;
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &filePath,
              CalibrationModel *model,
              QString *errorMessage = nullptr) const override;

    /// 先校验模型并重新计算 SHA-256，再通过 QSaveFile 原子写入 XML 1.4。
    bool save(const QString &filePath,
              const CalibrationModel &model,
              QString *errorMessage = nullptr) const;
    /// 生成不含 checksum 节点的稳定 XML 字节流，作为 SHA-256 输入。
    QByteArray canonicalPayload(const CalibrationModel &model) const;
};

/// 海康 XML 探测占位；当前没有官方解析契约，load 会明确返回 unsupported_format。
class HikXmlCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override { return QStringLiteral("hik_xml"); }
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &, CalibrationModel *, QString *errorMessage) const override;
};

/// 海康 IWCAL 探测占位；当前没有解析 SDK/格式契约，load 会明确返回 unsupported_format。
class HikIwcalCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override { return QStringLiteral("hik_iwcal"); }
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &, CalibrationModel *, QString *errorMessage) const override;
};

#endif // CALIBRATION_CALIBRATIONFILELOADER_H
