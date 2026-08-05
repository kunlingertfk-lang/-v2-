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
    for (const QPointF &point : model.validRegion) {
        xml.writeStartElement(QStringLiteral("point"));
        xml.writeAttribute(QStringLiteral("column"), QString::number(point.x(), 'g', 17));
        xml.writeAttribute(QStringLiteral("row"), QString::number(point.y(), 'g', 17));
        xml.writeEndElement();
    }
    xml.writeEndElement();

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
    return QStringLiteral("project_xml_1_0");
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
    bool rootSeen = false;
    while (!xml.atEnd()) {
        xml.readNext();
        if (!xml.isStartElement())
            continue;
        const QString name = xml.name().toString();
        if (name == QStringLiteral("CalibrationModel")) {
            rootSeen = true;
            parsed.schemaVersion = xml.attributes().value(QStringLiteral("schemaVersion")).toString();
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
            const QXmlStreamAttributes a = xml.attributes();
            if (!numberAttribute(a, QStringLiteral("meanError"), &parsed.quality.meanError)
                    || !numberAttribute(a, QStringLiteral("rmse"), &parsed.quality.rmse)
                    || !numberAttribute(a, QStringLiteral("maxError"), &parsed.quality.maxError)
                    || !numberAttribute(a, QStringLiteral("rmseX"), &parsed.quality.rmseX)
                    || !numberAttribute(a, QStringLiteral("rmseY"), &parsed.quality.rmseY)
                    || !numberAttribute(a, QStringLiteral("rmseLimit"), &parsed.quality.rmseLimit)
                    || !numberAttribute(a, QStringLiteral("maxErrorLimit"), &parsed.quality.maxErrorLimit)) {
                xml.raiseError(QStringLiteral("quality字段无效"));
            }
            parsed.quality.passed = a.value(QStringLiteral("passed")) == QStringLiteral("true");
        } else if (name == QStringLiteral("point")) {
            double column = 0.0;
            double row = 0.0;
            if (!numberAttribute(xml.attributes(), QStringLiteral("column"), &column)
                    || !numberAttribute(xml.attributes(), QStringLiteral("row"), &row)) {
                xml.raiseError(QStringLiteral("validRegion点无效"));
            }
            parsed.validRegion.append(QPointF(column, row));
        } else if (name == QStringLiteral("imageBinding")) {
            parsed.imageBinding = QJsonDocument::fromJson(xml.readElementText().toUtf8()).object();
        } else if (name == QStringLiteral("methodData")) {
            parsed.methodData = QJsonDocument::fromJson(xml.readElementText().toUtf8()).object();
        } else if (name == QStringLiteral("sample")) {
            const QXmlStreamAttributes a = xml.attributes();
            CalibrationSample sample;
            sample.index = a.value(QStringLiteral("index")).toInt();
            if (!numberAttribute(a, QStringLiteral("column"), &sample.column)
                    || !numberAttribute(a, QStringLiteral("row"), &sample.row)
                    || !numberAttribute(a, QStringLiteral("machineX"), &sample.machineX)
                    || !numberAttribute(a, QStringLiteral("machineY"), &sample.machineY)
                    || !numberAttribute(a, QStringLiteral("imageAngleDeg"), &sample.imageAngleDeg)
                    || !numberAttribute(a, QStringLiteral("machineAngleDeg"), &sample.machineAngleDeg)
                    || !numberAttribute(a, QStringLiteral("residualX"), &sample.residualX)
                    || !numberAttribute(a, QStringLiteral("residualY"), &sample.residualY)
                    || !numberAttribute(a, QStringLiteral("residual"), &sample.residual)) {
                xml.raiseError(QStringLiteral("sample字段无效"));
            }
            sample.source = a.value(QStringLiteral("source")).toString();
            sample.capturedAt = a.value(QStringLiteral("capturedAt")).toString();
            parsed.samples.append(sample);
        } else if (name == QStringLiteral("checksum")) {
            if (xml.attributes().value(QStringLiteral("algorithm")) != QStringLiteral("SHA-256"))
                xml.raiseError(QStringLiteral("不支持的校验算法"));
            parsed.checksum = xml.readElementText().trimmed().toLower();
        }
    }
    if (xml.hasError() || !rootSeen) {
        if (errorMessage)
            *errorMessage = xml.hasError() ? xml.errorString() : QStringLiteral("缺少CalibrationModel根节点");
        return false;
    }
    QString validationError;
    if (!parsed.isValid(&validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    const QString expected = QString::fromLatin1(QCryptographicHash::hash(
                                                      canonicalPayload(parsed),
                                                      QCryptographicHash::Sha256).toHex());
    if (parsed.checksum.isEmpty() || parsed.checksum != expected) {
        if (errorMessage)
            *errorMessage = QStringLiteral("标定文件SHA-256校验失败");
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
