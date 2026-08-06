#ifndef CALIBRATION_CALIBRATIONFILELOADER_H
#define CALIBRATION_CALIBRATIONFILELOADER_H

#include "calibration/CalibrationModel.h"

#include <QByteArray>
#include <QString>

class CalibrationFileLoader
{
public:
    virtual ~CalibrationFileLoader() = default;
    virtual QString formatId() const = 0;
    virtual bool canLoad(const QString &filePath) const = 0;
    virtual bool load(const QString &filePath,
                      CalibrationModel *model,
                      QString *errorMessage = nullptr) const = 0;
};

class ProjectXmlCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override;
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &filePath,
              CalibrationModel *model,
              QString *errorMessage = nullptr) const override;

    bool save(const QString &filePath,
              const CalibrationModel &model,
              QString *errorMessage = nullptr) const;
    QByteArray canonicalPayload(const CalibrationModel &model) const;
};

class HikXmlCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override { return QStringLiteral("hik_xml"); }
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &, CalibrationModel *, QString *errorMessage) const override;
};

class HikIwcalCalibrationLoader final : public CalibrationFileLoader
{
public:
    QString formatId() const override { return QStringLiteral("hik_iwcal"); }
    bool canLoad(const QString &filePath) const override;
    bool load(const QString &, CalibrationModel *, QString *errorMessage) const override;
};

#endif // CALIBRATION_CALIBRATIONFILELOADER_H
