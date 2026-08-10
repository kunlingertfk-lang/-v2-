#include "calibration/CalibrationFileLoader.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cmath>

namespace {

QString matrixText(const std::array<double, 6> &matrix)
{
    QStringList values;
    for (double value : matrix)
        values.append(QString::number(value, 'g', 17));
    return values.join(QLatin1Char(','));
}

bool parseMatrix(const QString &text, std::array<double, 6> *matrix)
{
    if (!matrix)
        return false;
    const QStringList values = text.split(QLatin1Char(','), Qt::SkipEmptyParts);
    if (values.size() != 6)
        return false;
    for (int i = 0; i < 6; ++i) {
        bool ok = false;
        const double value = values.at(i).trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return false;
        (*matrix)[static_cast<size_t>(i)] = value;
    }
    return true;
}

void writeModel(QXmlStreamWriter &xml,
                const CalibrationModel &model,
                bool includeChecksum)
{
    const bool schema11 = model.schemaVersion == QStringLiteral("1.1");
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("CalibrationModel"));
    xml.writeAttribute(QStringLiteral("schemaVersion"), model.schemaVersion);
    xml.writeTextElement(QStringLiteral("calibrationId"), model.calibrationId);
    xml.writeTextElement(QStringLiteral("methodId"), model.methodId);
    xml.writeTextElement(QStringLiteral("modelType"), model.modelType);
    xml.writeTextElement(QStringLiteral("coordinateConvention"), model.coordinateConvention);
    xml.writeTextElement(QStringLiteral("createdAt"), model.createdAt);
    xml.writeTextElement(QStringLiteral("forwardTransform"), matrixText(model.forward));
    xml.writeTextElement(QStringLiteral("inverseTransform"), matrixText(model.inverse));

    xml.writeStartElement(QStringLiteral("quality"));
    xml.writeAttribute(QStringLiteral("meanError"), QString::number(model.quality.meanError, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmse"), QString::number(model.quality.rmse, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxError"), QString::number(model.quality.maxError, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseX"), QString::number(model.quality.rmseX, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseY"), QString::number(model.quality.rmseY, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseLimit"), QString::number(model.quality.rmseLimit, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorLimit"), QString::number(model.quality.maxErrorLimit, 'g', 17));
    xml.writeAttribute(QStringLiteral("passed"), model.quality.passed ? QStringLiteral("true") : QStringLiteral("false"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("validRegion"));
    if (schema11) {
        xml.writeAttribute(QStringLiteral("coordinateSystem"),
                           model.validRegionCoordinateSystem);
        xml.writeAttribute(QStringLiteral("type"), model.validRegionType);
    }
    for (const QPointF &point : model.validRegion) {
        xml.writeStartElement(QStringLiteral("point"));
        xml.writeAttribute(QStringLiteral("column"), QString::number(point.x(), 'g', 17));
        xml.writeAttribute(QStringLiteral("row"), QString::number(point.y(), 'g', 17));
        xml.writeEndElement();
    }
    xml.writeEndElement();

    if (schema11) {
        xml.writeStartElement(QStringLiteral("safeRegion"));
        xml.writeAttribute(QStringLiteral("coordinateSystem"),
                           model.safeRegionCoordinateSystem);
        xml.writeAttribute(QStringLiteral("source"), model.safeRegionSource);
        xml.writeAttribute(QStringLiteral("type"), model.safeRegionType);
        xml.writeAttribute(QStringLiteral("marginPx"),
                           QString::number(model.safeMarginPx, 'g', 17));
        for (const QPointF &point : model.safeRegion) {
            xml.writeStartElement(QStringLiteral("point"));
            xml.writeAttribute(QStringLiteral("column"),
                               QString::number(point.x(), 'g', 17));
            xml.writeAttribute(QStringLiteral("row"),
                               QString::number(point.y(), 'g', 17));
            xml.writeEndElement();
        }
        xml.writeEndElement();

        xml.writeStartElement(QStringLiteral("rotationRange"));
        xml.writeAttribute(QStringLiteral("status"),
                           calibrationRotationRangeStatusToString(
                               model.rotationRange.status));
        xml.writeAttribute(QStringLiteral("valid"),
                           model.rotationRange.isVerified()
                           ? QStringLiteral("true") : QStringLiteral("false"));
        xml.writeAttribute(QStringLiteral("periodDeg"),
                           QString::number(model.rotationRange.periodDeg, 'g', 17));
        xml.writeStartElement(QStringLiteral("image"));
        xml.writeAttribute(QStringLiteral("minDeg"),
                           QString::number(model.rotationRange.imageMinDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("maxDeg"),
                           QString::number(model.rotationRange.imageMaxDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("centerDeg"),
                           QString::number(model.rotationRange.imageCenterDeg, 'g', 17));
        xml.writeEndElement();
        xml.writeStartElement(QStringLiteral("machine"));
        xml.writeAttribute(QStringLiteral("minDeg"),
                           QString::number(model.rotationRange.machineMinDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("maxDeg"),
                           QString::number(model.rotationRange.machineMaxDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("centerDeg"),
                           QString::number(model.rotationRange.machineCenterDeg, 'g', 17));
        xml.writeEndElement();
        xml.writeEndElement();
    }

    xml.writeStartElement(QStringLiteral("imageBinding"));
    xml.writeCDATA(QString::fromUtf8(QJsonDocument(model.imageBinding).toJson(QJsonDocument::Compact)));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("methodData"));
    xml.writeCDATA(QString::fromUtf8(QJsonDocument(model.methodData).toJson(QJsonDocument::Compact)));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("samples"));
    for (const CalibrationSample &sample : model.samples) {
        xml.writeStartElement(QStringLiteral("sample"));
        xml.writeAttribute(QStringLiteral("index"), QString::number(sample.index));
        xml.writeAttribute(QStringLiteral("column"), QString::number(sample.column, 'g', 17));
        xml.writeAttribute(QStringLiteral("row"), QString::number(sample.row, 'g', 17));
        xml.writeAttribute(QStringLiteral("machineX"), QString::number(sample.machineX, 'g', 17));
        xml.writeAttribute(QStringLiteral("machineY"), QString::number(sample.machineY, 'g', 17));
        xml.writeAttribute(QStringLiteral("imageAngleDeg"), QString::number(sample.imageAngleDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("machineAngleDeg"), QString::number(sample.machineAngleDeg, 'g', 17));
        xml.writeAttribute(QStringLiteral("residualX"), QString::number(sample.residualX, 'g', 17));
        xml.writeAttribute(QStringLiteral("residualY"), QString::number(sample.residualY, 'g', 17));
        xml.writeAttribute(QStringLiteral("residual"), QString::number(sample.residual, 'g', 17));
        xml.writeAttribute(QStringLiteral("source"), sample.source);
        xml.writeAttribute(QStringLiteral("capturedAt"), sample.capturedAt);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    if (includeChecksum) {
        xml.writeStartElement(QStringLiteral("checksum"));
        xml.writeAttribute(QStringLiteral("algorithm"), QStringLiteral("SHA-256"));
        xml.writeCharacters(model.checksum);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
}

bool numberAttribute(const QXmlStreamAttributes &attributes,
                     const QString &key,
                     double *value)
{
    if (!value)
        return false;
    bool ok = false;
    const double parsed = attributes.value(key).toString().toDouble(&ok);
    if (!ok || !std::isfinite(parsed))
        return false;
    *value = parsed;
    return true;
}

} // namespace

QString ProjectXmlCalibrationLoader::formatId() const
{
    return QStringLiteral("project_xml_1_1");
}

bool ProjectXmlCalibrationLoader::canLoad(const QString &filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QByteArray prefix = file.read(4096);
    return prefix.contains("<CalibrationModel");
}

QByteArray ProjectXmlCalibrationLoader::canonicalPayload(
        const CalibrationModel &model) const
{
    QByteArray bytes;
    QXmlStreamWriter writer(&bytes);
    writer.setAutoFormatting(false);
    writeModel(writer, model, false);
    return bytes;
}

bool ProjectXmlCalibrationLoader::save(const QString &filePath,
                                       const CalibrationModel &input,
                                       QString *errorMessage) const
{
    CalibrationModel model = input;
    if (model.schemaVersion != QStringLiteral("1.0")
            && model.schemaVersion != QStringLiteral("1.1")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("unsupported schema version: %1")
                    .arg(model.schemaVersion);
        }
        return false;
    }
    if (model.safeRegion.isEmpty() && model.safeMarginPx == 0.0)
        model.safeRegion = model.validRegion;
    model.schemaVersion = QStringLiteral("1.1");
    QString validationError;
    if (!model.isValid(&validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    model.checksum = QString::fromLatin1(QCryptographicHash::hash(
                                             canonicalPayload(model),
                                             QCryptographicHash::Sha256).toHex());
    QByteArray bytes;
    QXmlStreamWriter writer(&bytes);
    writer.setAutoFormatting(true);
    writeModel(writer, model, true);
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)
            || file.write(bytes) != bytes.size()
            || !file.commit()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法原子写入标定文件: %1").arg(filePath);
        return false;
    }
    return true;
}

bool ProjectXmlCalibrationLoader::load(const QString &filePath,
                                       CalibrationModel *model,
                                       QString *errorMessage) const
{
    if (!model) {
        if (errorMessage)
            *errorMessage = QStringLiteral("输出模型指针为空");
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法打开标定文件: %1").arg(filePath);
        return false;
    }
    QXmlStreamReader xml(&file);
    CalibrationModel parsed;
    enum class RegionParent { None, Valid, Safe };
    RegionParent regionParent = RegionParent::None;
    bool insideRotationRange = false;
    bool rootSeen = false;
    bool validRegionSeen = false;
    bool safeRegionSeen = false;
    bool rotationRangeSeen = false;
    bool rotationImageSeen = false;
    bool rotationMachineSeen = false;

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement()) {
            const QString name = xml.name().toString();
            if (name == QStringLiteral("validRegion")
                    || name == QStringLiteral("safeRegion")) {
                regionParent = RegionParent::None;
            } else if (name == QStringLiteral("rotationRange")) {
                insideRotationRange = false;
            }
            continue;
        }
        if (!xml.isStartElement())
            continue;

        const QString name = xml.name().toString();
        const QXmlStreamAttributes attributes = xml.attributes();
        if (regionParent != RegionParent::None
                && name != QStringLiteral("point")) {
            xml.raiseError(QStringLiteral("区域节点只允许直接包含point"));
            continue;
        }
        if (name == QStringLiteral("CalibrationModel")) {
            if (rootSeen) {
                xml.raiseError(QStringLiteral("CalibrationModel根节点重复"));
                continue;
            }
            rootSeen = true;
            parsed.schemaVersion = attributes.value(
                        QStringLiteral("schemaVersion")).toString();
            if (parsed.schemaVersion != QStringLiteral("1.0")
                    && parsed.schemaVersion != QStringLiteral("1.1")) {
                xml.raiseError(QStringLiteral("unsupported schema version: %1")
                               .arg(parsed.schemaVersion));
            }
        } else if (name == QStringLiteral("calibrationId")) {
            parsed.calibrationId = xml.readElementText();
        } else if (name == QStringLiteral("methodId")) {
            parsed.methodId = xml.readElementText();
        } else if (name == QStringLiteral("modelType")) {
            parsed.modelType = xml.readElementText();
        } else if (name == QStringLiteral("coordinateConvention")) {
            parsed.coordinateConvention = xml.readElementText();
        } else if (name == QStringLiteral("createdAt")) {
            parsed.createdAt = xml.readElementText();
        } else if (name == QStringLiteral("forwardTransform")) {
            if (!parseMatrix(xml.readElementText(), &parsed.forward))
                xml.raiseError(QStringLiteral("forwardTransform格式无效"));
        } else if (name == QStringLiteral("inverseTransform")) {
            if (!parseMatrix(xml.readElementText(), &parsed.inverse))
                xml.raiseError(QStringLiteral("inverseTransform格式无效"));
        } else if (name == QStringLiteral("quality")) {
            if (!numberAttribute(attributes, QStringLiteral("meanError"),
                                 &parsed.quality.meanError)
                    || !numberAttribute(attributes, QStringLiteral("rmse"),
                                        &parsed.quality.rmse)
                    || !numberAttribute(attributes, QStringLiteral("maxError"),
                                        &parsed.quality.maxError)
                    || !numberAttribute(attributes, QStringLiteral("rmseX"),
                                        &parsed.quality.rmseX)
                    || !numberAttribute(attributes, QStringLiteral("rmseY"),
                                        &parsed.quality.rmseY)
                    || !numberAttribute(attributes, QStringLiteral("rmseLimit"),
                                        &parsed.quality.rmseLimit)
                    || !numberAttribute(attributes, QStringLiteral("maxErrorLimit"),
                                        &parsed.quality.maxErrorLimit)) {
                xml.raiseError(QStringLiteral("quality字段无效"));
            }
            parsed.quality.passed = attributes.value(QStringLiteral("passed"))
                    == QStringLiteral("true");
        } else if (name == QStringLiteral("validRegion")) {
            if (validRegionSeen) {
                xml.raiseError(QStringLiteral("validRegion节点重复"));
                continue;
            }
            validRegionSeen = true;
            regionParent = RegionParent::Valid;
            if (parsed.schemaVersion == QStringLiteral("1.1")) {
                parsed.validRegionCoordinateSystem = attributes.value(
                            QStringLiteral("coordinateSystem")).toString();
                parsed.validRegionType = attributes.value(
                            QStringLiteral("type")).toString();
            }
        } else if (name == QStringLiteral("safeRegion")) {
            if (parsed.schemaVersion != QStringLiteral("1.1")) {
                xml.raiseError(QStringLiteral("XML 1.0不允许safeRegion"));
                continue;
            }
            if (safeRegionSeen) {
                xml.raiseError(QStringLiteral("safeRegion节点重复"));
                continue;
            }
            safeRegionSeen = true;
            regionParent = RegionParent::Safe;
            parsed.safeRegionCoordinateSystem = attributes.value(
                        QStringLiteral("coordinateSystem")).toString();
            parsed.safeRegionSource = attributes.value(
                        QStringLiteral("source")).toString();
            parsed.safeRegionType = attributes.value(
                        QStringLiteral("type")).toString();
            if (!numberAttribute(attributes, QStringLiteral("marginPx"),
                                 &parsed.safeMarginPx)) {
                xml.raiseError(QStringLiteral("safeRegion marginPx无效"));
            }
        } else if (name == QStringLiteral("point")) {
            double column = 0.0;
            double row = 0.0;
            if (regionParent == RegionParent::None) {
                xml.raiseError(QStringLiteral("区域point缺少合法父节点"));
                continue;
            }
            if (!numberAttribute(attributes, QStringLiteral("column"), &column)
                    || !numberAttribute(attributes, QStringLiteral("row"), &row)) {
                xml.raiseError(QStringLiteral("区域point无效"));
                continue;
            }
            if (regionParent == RegionParent::Valid)
                parsed.validRegion.append(QPointF(column, row));
            else
                parsed.safeRegion.append(QPointF(column, row));
        } else if (name == QStringLiteral("rotationRange")) {
            if (parsed.schemaVersion != QStringLiteral("1.1")) {
                xml.raiseError(QStringLiteral("XML 1.0不允许rotationRange"));
                continue;
            }
            if (rotationRangeSeen) {
                xml.raiseError(QStringLiteral("rotationRange节点重复"));
                continue;
            }
            rotationRangeSeen = true;
            insideRotationRange = true;
            const QString statusText = attributes.value(
                        QStringLiteral("status")).toString();
            if (!calibrationRotationRangeStatusFromString(
                        statusText, &parsed.rotationRange.status)
                    || !numberAttribute(attributes, QStringLiteral("periodDeg"),
                                        &parsed.rotationRange.periodDeg)) {
                xml.raiseError(QStringLiteral("rotationRange字段无效"));
            }
            const QString validText = attributes.value(
                        QStringLiteral("valid")).toString();
            if (!validText.isEmpty()) {
                const bool valid = validText == QStringLiteral("true");
                if ((validText != QStringLiteral("true")
                     && validText != QStringLiteral("false"))
                        || valid != parsed.rotationRange.isVerified()) {
                    xml.raiseError(QStringLiteral("rotationRange valid与status不一致"));
                }
            }
        } else if (name == QStringLiteral("image") && insideRotationRange) {
            if (rotationImageSeen) {
                xml.raiseError(QStringLiteral("rotationRange image节点重复"));
                continue;
            }
            rotationImageSeen = true;
            if (!numberAttribute(attributes, QStringLiteral("minDeg"),
                                 &parsed.rotationRange.imageMinDeg)
                    || !numberAttribute(attributes, QStringLiteral("maxDeg"),
                                        &parsed.rotationRange.imageMaxDeg)
                    || !numberAttribute(attributes, QStringLiteral("centerDeg"),
                                        &parsed.rotationRange.imageCenterDeg)) {
                xml.raiseError(QStringLiteral("rotationRange image字段无效"));
            }
        } else if (name == QStringLiteral("machine") && insideRotationRange) {
            if (rotationMachineSeen) {
                xml.raiseError(QStringLiteral("rotationRange machine节点重复"));
                continue;
            }
            rotationMachineSeen = true;
            if (!numberAttribute(attributes, QStringLiteral("minDeg"),
                                 &parsed.rotationRange.machineMinDeg)
                    || !numberAttribute(attributes, QStringLiteral("maxDeg"),
                                        &parsed.rotationRange.machineMaxDeg)
                    || !numberAttribute(attributes, QStringLiteral("centerDeg"),
                                        &parsed.rotationRange.machineCenterDeg)) {
                xml.raiseError(QStringLiteral("rotationRange machine字段无效"));
            }
        } else if (name == QStringLiteral("imageBinding")) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(
                        xml.readElementText().toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
                xml.raiseError(QStringLiteral("imageBinding JSON无效"));
            else
                parsed.imageBinding = document.object();
        } else if (name == QStringLiteral("methodData")) {
            QJsonParseError parseError;
            const QJsonDocument document = QJsonDocument::fromJson(
                        xml.readElementText().toUtf8(), &parseError);
            if (parseError.error != QJsonParseError::NoError || !document.isObject())
                xml.raiseError(QStringLiteral("methodData JSON无效"));
            else
                parsed.methodData = document.object();
        } else if (name == QStringLiteral("sample")) {
            CalibrationSample sample;
            bool indexOk = false;
            sample.index = attributes.value(QStringLiteral("index"))
                    .toString().toInt(&indexOk);
            if (!indexOk
                    || !numberAttribute(attributes, QStringLiteral("column"), &sample.column)
                    || !numberAttribute(attributes, QStringLiteral("row"), &sample.row)
                    || !numberAttribute(attributes, QStringLiteral("machineX"), &sample.machineX)
                    || !numberAttribute(attributes, QStringLiteral("machineY"), &sample.machineY)
                    || !numberAttribute(attributes, QStringLiteral("imageAngleDeg"),
                                        &sample.imageAngleDeg)
                    || !numberAttribute(attributes, QStringLiteral("machineAngleDeg"),
                                        &sample.machineAngleDeg)
                    || !numberAttribute(attributes, QStringLiteral("residualX"),
                                        &sample.residualX)
                    || !numberAttribute(attributes, QStringLiteral("residualY"),
                                        &sample.residualY)
                    || !numberAttribute(attributes, QStringLiteral("residual"),
                                        &sample.residual)) {
                xml.raiseError(QStringLiteral("sample字段无效"));
            }
            sample.source = attributes.value(QStringLiteral("source")).toString();
            sample.capturedAt = attributes.value(QStringLiteral("capturedAt")).toString();
            parsed.samples.append(sample);
        } else if (name == QStringLiteral("checksum")) {
            if (attributes.value(QStringLiteral("algorithm"))
                    != QStringLiteral("SHA-256")) {
                xml.raiseError(QStringLiteral("不支持的校验算法"));
            }
            parsed.checksum = xml.readElementText().trimmed().toLower();
        }
    }
    if (xml.hasError() || !rootSeen) {
        if (errorMessage)
            *errorMessage = xml.hasError() ? xml.errorString() : QStringLiteral("缺少CalibrationModel根节点");
        return false;
    }

    if (!validRegionSeen
            || (parsed.schemaVersion == QStringLiteral("1.1")
                && (!safeRegionSeen || !rotationRangeSeen
                    || !rotationImageSeen || !rotationMachineSeen))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("标定文件缺少版本要求的区域或旋转节点");
        return false;
    }

    // Verify 1.0 with the exact legacy canonical payload before adding any
    // 1.1 defaults.  Otherwise every existing signed calibration would fail.
    const QString expected = QString::fromLatin1(QCryptographicHash::hash(
                                                      canonicalPayload(parsed),
                                                      QCryptographicHash::Sha256).toHex());
    if (parsed.checksum.isEmpty() || parsed.checksum != expected) {
        if (errorMessage)
            *errorMessage = QStringLiteral("标定文件SHA-256校验失败");
        return false;
    }

    if (parsed.schemaVersion == QStringLiteral("1.0")) {
        parsed.safeRegion = parsed.validRegion;
        parsed.safeMarginPx = 0.0;
        parsed.rotationRange = CalibrationRotationRange();
    }
    QString validationError;
    if (!parsed.isValid(&validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    *model = parsed;
    return true;
}

bool HikXmlCalibrationLoader::canLoad(const QString &filePath) const
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("xml"), Qt::CaseInsensitive) == 0
            && !ProjectXmlCalibrationLoader().canLoad(filePath);
}

bool HikXmlCalibrationLoader::load(const QString &, CalibrationModel *, QString *errorMessage) const
{
    if (errorMessage)
        *errorMessage = QStringLiteral("unsupported_format: 尚未取得海康XML样例或官方解析契约");
    return false;
}

bool HikIwcalCalibrationLoader::canLoad(const QString &filePath) const
{
    return QFileInfo(filePath).suffix().compare(QStringLiteral("iwcal"), Qt::CaseInsensitive) == 0;
}

bool HikIwcalCalibrationLoader::load(const QString &, CalibrationModel *, QString *errorMessage) const
{
    if (errorMessage)
        *errorMessage = QStringLiteral("unsupported_format: 尚未取得海康IWCAL解析SDK或格式契约");
    return false;
}
