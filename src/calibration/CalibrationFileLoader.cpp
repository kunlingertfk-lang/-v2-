#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationSolver.h"

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
    const QStringList values = text.split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (values.size() != 6)
        return false;
    for (int index = 0; index < values.size(); ++index) {
        bool ok = false;
        const double value = values.at(index).trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value))
            return false;
        (*matrix)[static_cast<size_t>(index)] = value;
    }
    return true;
}

bool numberAttribute(const QXmlStreamAttributes &attributes,
                     const QString &name,
                     double *output)
{
    if (!output || !attributes.hasAttribute(name))
        return false;
    bool ok = false;
    const double value = attributes.value(name).toString().toDouble(&ok);
    if (!ok || !std::isfinite(value))
        return false;
    *output = value;
    return true;
}

bool integerAttribute(const QXmlStreamAttributes &attributes,
                      const QString &name,
                      int *output)
{
    if (!output || !attributes.hasAttribute(name))
        return false;
    bool ok = false;
    const int value = attributes.value(name).toString().toInt(&ok);
    if (!ok || value < 0)
        return false;
    *output = value;
    return true;
}

bool booleanAttribute(const QXmlStreamAttributes &attributes,
                      const QString &name,
                      bool *output)
{
    if (!output || !attributes.hasAttribute(name))
        return false;
    const QString value = attributes.value(name).toString().trimmed().toLower();
    if (value == QStringLiteral("true")) {
        *output = true;
        return true;
    }
    if (value == QStringLiteral("false")) {
        *output = false;
        return true;
    }
    return false;
}

void writePoint(QXmlStreamWriter &xml, const QPointF &point)
{
    xml.writeStartElement(QStringLiteral("point"));
    xml.writeAttribute(QStringLiteral("column"),
                       QString::number(point.x(), 'g', 17));
    xml.writeAttribute(QStringLiteral("row"),
                       QString::number(point.y(), 'g', 17));
    xml.writeEndElement();
}

void writeSample(QXmlStreamWriter &xml,
                 const CalibrationSample &sample,
                 bool includeResidual)
{
    xml.writeStartElement(QStringLiteral("sample"));
    xml.writeAttribute(QStringLiteral("index"), QString::number(sample.index));
    xml.writeAttribute(QStringLiteral("column"),
                       QString::number(sample.column, 'g', 17));
    xml.writeAttribute(QStringLiteral("row"),
                       QString::number(sample.row, 'g', 17));
    xml.writeAttribute(QStringLiteral("machineX"),
                       QString::number(sample.machineX, 'g', 17));
    xml.writeAttribute(QStringLiteral("machineY"),
                       QString::number(sample.machineY, 'g', 17));
    xml.writeAttribute(QStringLiteral("imageAngleDeg"),
                       QString::number(sample.imageAngleDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("machineAngleDeg"),
                       QString::number(sample.machineAngleDeg, 'g', 17));
    if (includeResidual) {
        xml.writeAttribute(QStringLiteral("residualX"),
                           QString::number(sample.residualX, 'g', 17));
        xml.writeAttribute(QStringLiteral("residualY"),
                           QString::number(sample.residualY, 'g', 17));
        xml.writeAttribute(QStringLiteral("residual"),
                           QString::number(sample.residual, 'g', 17));
    }
    xml.writeAttribute(QStringLiteral("source"), sample.source);
    xml.writeAttribute(QStringLiteral("capturedAt"), sample.capturedAt);
    xml.writeEndElement();
}

void writeModel(QXmlStreamWriter &xml,
                const CalibrationModel &model,
                bool includeChecksum)
{
    xml.writeStartDocument(QStringLiteral("1.0"));
    xml.writeStartElement(QStringLiteral("calibration"));
    xml.writeAttribute(QStringLiteral("schemaVersion"),
                       QStringLiteral("1.4"));
    xml.writeAttribute(QStringLiteral("mode"),
                       nPointCalibrationModeToString(model.mode));

    xml.writeStartElement(QStringLiteral("identity"));
    xml.writeAttribute(QStringLiteral("calibrationId"), model.calibrationId);
    xml.writeAttribute(QStringLiteral("createdAt"), model.createdAt);
    xml.writeEndElement();

    xml.writeTextElement(QStringLiteral("methodId"), model.methodId);
    xml.writeTextElement(QStringLiteral("modelType"), model.modelType);
    xml.writeTextElement(QStringLiteral("coordinateConvention"),
                         model.coordinateConvention);

    xml.writeStartElement(QStringLiteral("transform"));
    xml.writeAttribute(QStringLiteral("model"), model.modelType);
    xml.writeAttribute(QStringLiteral("parity"),
                       calibrationTransformParityToString(
                           model.transformParity));
    xml.writeTextElement(QStringLiteral("forward"), matrixText(model.forward));
    xml.writeTextElement(QStringLiteral("inverse"), matrixText(model.inverse));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("sampleSummary"));
    xml.writeAttribute(QStringLiteral("translationCount"),
                       QString::number(model.translationSampleCount));
    xml.writeAttribute(QStringLiteral("rotationCount"),
                       QString::number(model.configuredRotationSampleCount));
    xml.writeAttribute(QStringLiteral("totalCount"),
                       QString::number(model.totalSampleCount));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("translationSamples"));
    for (const CalibrationSample &sample : model.samples)
        writeSample(xml, sample, true);
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("rotationSamples"));
    for (const CalibrationSample &sample : model.rotationSamples)
        writeSample(xml, sample, false);
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("validRegion"));
    xml.writeAttribute(QStringLiteral("coordinateSystem"),
                       model.validRegionCoordinateSystem);
    xml.writeAttribute(QStringLiteral("type"), model.validRegionType);
    for (const QPointF &point : model.validRegion)
        writePoint(xml, point);
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("safeRegion"));
    xml.writeAttribute(QStringLiteral("coordinateSystem"),
                       model.safeRegionCoordinateSystem);
    xml.writeAttribute(QStringLiteral("source"), model.safeRegionSource);
    xml.writeAttribute(QStringLiteral("type"), model.safeRegionType);
    xml.writeAttribute(QStringLiteral("marginPx"),
                       QString::number(model.safeMarginPx, 'g', 17));
    for (const QPointF &point : model.safeRegion)
        writePoint(xml, point);
    xml.writeEndElement();

    const CalibrationRotationRange &trace = model.rotationRange;
    xml.writeStartElement(QStringLiteral("axisTrace"));
    xml.writeAttribute(QStringLiteral("status"),
                       calibrationRotationRangeStatusToString(trace.status));
    xml.writeAttribute(QStringLiteral("sampleCount"),
                       QString::number(trace.sampleCount));
    xml.writeAttribute(QStringLiteral("direction"),
                       calibrationRotationDirectionToString(trace.direction));
    xml.writeAttribute(QStringLiteral("coaxial"),
                       trace.coaxial ? QStringLiteral("true")
                                     : QStringLiteral("false"));
    xml.writeAttribute(QStringLiteral("periodDeg"),
                       QString::number(trace.periodDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerOffsetX"),
                       QString::number(trace.centerOffsetX, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerOffsetY"),
                       QString::number(trace.centerOffsetY, 'g', 17));
    xml.writeAttribute(QStringLiteral("radiusMm"),
                       QString::number(trace.radiusMm, 'g', 17));
    xml.writeAttribute(QStringLiteral("phaseOffsetDeg"),
                       QString::number(trace.phaseOffsetDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("fitRmseMm"),
                       QString::number(trace.fitRmseMm, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorMm"),
                       QString::number(trace.maxErrorMm, 'g', 17));
    xml.writeAttribute(QStringLiteral("minimumSpanDeg"),
                       QString::number(trace.minimumSpanDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseLimitMm"),
                       QString::number(trace.rmseLimitMm, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorLimitMm"),
                       QString::number(trace.maxErrorLimitMm, 'g', 17));
    xml.writeAttribute(QStringLiteral("validationCode"),
                       trace.validationCode);
    xml.writeStartElement(QStringLiteral("trajectory"));
    xml.writeAttribute(QStringLiteral("minDeg"),
                       QString::number(trace.trajectoryMinDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxDeg"),
                       QString::number(trace.trajectoryMaxDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerDeg"),
                       QString::number(trace.trajectoryCenterDeg, 'g', 17));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("machine"));
    xml.writeAttribute(QStringLiteral("minDeg"),
                       QString::number(trace.machineMinDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxDeg"),
                       QString::number(trace.machineMaxDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerDeg"),
                       QString::number(trace.machineCenterDeg, 'g', 17));
    xml.writeEndElement();
    xml.writeEndElement();

    const CalibrationAngleMapping &mapping = model.angleMapping;
    xml.writeStartElement(QStringLiteral("angleMapping"));
    xml.writeAttribute(QStringLiteral("status"),
                       calibrationAngleMappingStatusToString(mapping.status));
    xml.writeAttribute(QStringLiteral("sampleCount"),
                       QString::number(mapping.sampleCount));
    xml.writeAttribute(QStringLiteral("direction"),
                       calibrationAngleDirectionToString(mapping.direction));
    xml.writeAttribute(QStringLiteral("offsetDeg"),
                       QString::number(mapping.offsetDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("periodDeg"),
                       QString::number(mapping.periodDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseDeg"),
                       QString::number(mapping.rmseDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorDeg"),
                       QString::number(mapping.maxErrorDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("minimumSpanDeg"),
                       QString::number(mapping.minimumSpanDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseLimitDeg"),
                       QString::number(mapping.rmseLimitDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorLimitDeg"),
                       QString::number(mapping.maxErrorLimitDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("validationCode"),
                       mapping.validationCode);
    xml.writeStartElement(QStringLiteral("image"));
    xml.writeAttribute(QStringLiteral("minDeg"),
                       QString::number(mapping.imageMinDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxDeg"),
                       QString::number(mapping.imageMaxDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerDeg"),
                       QString::number(mapping.imageCenterDeg, 'g', 17));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("machine"));
    xml.writeAttribute(QStringLiteral("minDeg"),
                       QString::number(mapping.machineMinDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxDeg"),
                       QString::number(mapping.machineMaxDeg, 'g', 17));
    xml.writeAttribute(QStringLiteral("centerDeg"),
                       QString::number(mapping.machineCenterDeg, 'g', 17));
    xml.writeEndElement();
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("quality"));
    xml.writeAttribute(QStringLiteral("meanError"),
                       QString::number(model.quality.meanError, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmse"),
                       QString::number(model.quality.rmse, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxError"),
                       QString::number(model.quality.maxError, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseX"),
                       QString::number(model.quality.rmseX, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseY"),
                       QString::number(model.quality.rmseY, 'g', 17));
    xml.writeAttribute(QStringLiteral("rmseLimit"),
                       QString::number(model.quality.rmseLimit, 'g', 17));
    xml.writeAttribute(QStringLiteral("maxErrorLimit"),
                       QString::number(model.quality.maxErrorLimit, 'g', 17));
    xml.writeAttribute(QStringLiteral("passed"),
                       model.quality.passed ? QStringLiteral("true")
                                            : QStringLiteral("false"));
    xml.writeEndElement();

    xml.writeStartElement(QStringLiteral("sourceFingerprint"));
    xml.writeCDATA(QString::fromUtf8(
                       QJsonDocument(model.imageBinding)
                       .toJson(QJsonDocument::Compact)));
    xml.writeEndElement();
    xml.writeStartElement(QStringLiteral("methodData"));
    xml.writeCDATA(QString::fromUtf8(
                       QJsonDocument(model.methodData)
                       .toJson(QJsonDocument::Compact)));
    xml.writeEndElement();

    if (includeChecksum) {
        xml.writeStartElement(QStringLiteral("checksum"));
        xml.writeAttribute(QStringLiteral("algorithm"),
                           QStringLiteral("SHA-256"));
        xml.writeCharacters(model.checksum);
        xml.writeEndElement();
    }
    xml.writeEndElement();
    xml.writeEndDocument();
}

bool parsePointRegion(QXmlStreamReader &xml,
                      QVector<QPointF> *points,
                      const QString &expectedEnd)
{
    points->clear();
    while (xml.readNextStartElement()) {
        if (xml.name() != QStringLiteral("point")) {
            xml.raiseError(QStringLiteral("%1 contains an unexpected node")
                           .arg(expectedEnd));
            return false;
        }
        double column = 0.0;
        double row = 0.0;
        if (!numberAttribute(xml.attributes(), QStringLiteral("column"),
                             &column)
                || !numberAttribute(xml.attributes(), QStringLiteral("row"),
                                    &row)) {
            xml.raiseError(QStringLiteral("invalid region point"));
            return false;
        }
        points->append(QPointF(column, row));
        xml.skipCurrentElement();
    }
    return !xml.hasError();
}

bool parseSample(const QXmlStreamAttributes &attributes,
                 bool includeResidual,
                 CalibrationSample *sample)
{
    return sample
            && integerAttribute(attributes, QStringLiteral("index"),
                                &sample->index)
            && numberAttribute(attributes, QStringLiteral("column"),
                               &sample->column)
            && numberAttribute(attributes, QStringLiteral("row"),
                               &sample->row)
            && numberAttribute(attributes, QStringLiteral("machineX"),
                               &sample->machineX)
            && numberAttribute(attributes, QStringLiteral("machineY"),
                               &sample->machineY)
            && numberAttribute(attributes, QStringLiteral("imageAngleDeg"),
                               &sample->imageAngleDeg)
            && numberAttribute(attributes, QStringLiteral("machineAngleDeg"),
                               &sample->machineAngleDeg)
            && (!includeResidual
                || (numberAttribute(attributes, QStringLiteral("residualX"),
                                    &sample->residualX)
                    && numberAttribute(attributes, QStringLiteral("residualY"),
                                       &sample->residualY)
                    && numberAttribute(attributes, QStringLiteral("residual"),
                                       &sample->residual)));
}

bool parseSamples(QXmlStreamReader &xml,
                  bool includeResidual,
                  QVector<CalibrationSample> *samples)
{
    samples->clear();
    while (xml.readNextStartElement()) {
        if (xml.name() != QStringLiteral("sample")) {
            xml.raiseError(QStringLiteral("unexpected sample list node"));
            return false;
        }
        CalibrationSample sample;
        if (!parseSample(xml.attributes(), includeResidual, &sample)) {
            xml.raiseError(QStringLiteral("invalid sample fields"));
            return false;
        }
        sample.source = xml.attributes().value(
                    QStringLiteral("source")).toString();
        sample.capturedAt = xml.attributes().value(
                    QStringLiteral("capturedAt")).toString();
        samples->append(sample);
        xml.skipCurrentElement();
    }
    return !xml.hasError();
}

bool parseAngleRange(const QXmlStreamAttributes &attributes,
                     double *minimum,
                     double *maximum,
                     double *center)
{
    return numberAttribute(attributes, QStringLiteral("minDeg"), minimum)
            && numberAttribute(attributes, QStringLiteral("maxDeg"), maximum)
            && numberAttribute(attributes, QStringLiteral("centerDeg"), center);
}

bool parseAxisTrace(QXmlStreamReader &xml, CalibrationRotationRange *trace)
{
    const QXmlStreamAttributes attributes = xml.attributes();
    if (!calibrationRotationRangeStatusFromString(
            attributes.value(QStringLiteral("status")).toString(),
            &trace->status)
            || !integerAttribute(attributes, QStringLiteral("sampleCount"),
                                 &trace->sampleCount)
            || !calibrationRotationDirectionFromString(
                attributes.value(QStringLiteral("direction")).toString(),
                &trace->direction)
            || !booleanAttribute(attributes, QStringLiteral("coaxial"),
                                 &trace->coaxial)
            || !numberAttribute(attributes, QStringLiteral("periodDeg"),
                                &trace->periodDeg)
            || !numberAttribute(attributes, QStringLiteral("centerOffsetX"),
                                &trace->centerOffsetX)
            || !numberAttribute(attributes, QStringLiteral("centerOffsetY"),
                                &trace->centerOffsetY)
            || !numberAttribute(attributes, QStringLiteral("radiusMm"),
                                &trace->radiusMm)
            || !numberAttribute(attributes, QStringLiteral("phaseOffsetDeg"),
                                &trace->phaseOffsetDeg)
            || !numberAttribute(attributes, QStringLiteral("fitRmseMm"),
                                &trace->fitRmseMm)
            || !numberAttribute(attributes, QStringLiteral("maxErrorMm"),
                                &trace->maxErrorMm)
            || !numberAttribute(attributes, QStringLiteral("minimumSpanDeg"),
                                &trace->minimumSpanDeg)
            || !numberAttribute(attributes, QStringLiteral("rmseLimitMm"),
                                &trace->rmseLimitMm)
            || !numberAttribute(attributes, QStringLiteral("maxErrorLimitMm"),
                                &trace->maxErrorLimitMm)) {
        xml.raiseError(QStringLiteral("invalid axisTrace attributes"));
        return false;
    }
    trace->validationCode = attributes.value(
                QStringLiteral("validationCode")).toString();
    bool trajectorySeen = false;
    bool machineSeen = false;
    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("trajectory") && !trajectorySeen) {
            trajectorySeen = true;
            if (!parseAngleRange(xml.attributes(), &trace->trajectoryMinDeg,
                                 &trace->trajectoryMaxDeg,
                                 &trace->trajectoryCenterDeg)) {
                xml.raiseError(QStringLiteral("invalid axis trajectory range"));
                return false;
            }
            xml.skipCurrentElement();
        } else if (xml.name() == QStringLiteral("machine") && !machineSeen) {
            machineSeen = true;
            if (!parseAngleRange(xml.attributes(), &trace->machineMinDeg,
                                 &trace->machineMaxDeg,
                                 &trace->machineCenterDeg)) {
                xml.raiseError(QStringLiteral("invalid axis machine range"));
                return false;
            }
            xml.skipCurrentElement();
        } else {
            xml.raiseError(QStringLiteral("unexpected or duplicate axisTrace node"));
            return false;
        }
    }
    if (!trajectorySeen || !machineSeen) {
        xml.raiseError(QStringLiteral("axisTrace range nodes are incomplete"));
        return false;
    }
    return true;
}

bool parseAngleMapping(QXmlStreamReader &xml,
                       CalibrationAngleMapping *mapping)
{
    const QXmlStreamAttributes attributes = xml.attributes();
    if (!calibrationAngleMappingStatusFromString(
            attributes.value(QStringLiteral("status")).toString(),
            &mapping->status)
            || !integerAttribute(attributes, QStringLiteral("sampleCount"),
                                 &mapping->sampleCount)
            || !calibrationAngleDirectionFromString(
                attributes.value(QStringLiteral("direction")).toString(),
                &mapping->direction)
            || !numberAttribute(attributes, QStringLiteral("offsetDeg"),
                                &mapping->offsetDeg)
            || !numberAttribute(attributes, QStringLiteral("periodDeg"),
                                &mapping->periodDeg)
            || !numberAttribute(attributes, QStringLiteral("rmseDeg"),
                                &mapping->rmseDeg)
            || !numberAttribute(attributes, QStringLiteral("maxErrorDeg"),
                                &mapping->maxErrorDeg)
            || !numberAttribute(attributes, QStringLiteral("minimumSpanDeg"),
                                &mapping->minimumSpanDeg)
            || !numberAttribute(attributes, QStringLiteral("rmseLimitDeg"),
                                &mapping->rmseLimitDeg)
            || !numberAttribute(attributes, QStringLiteral("maxErrorLimitDeg"),
                                &mapping->maxErrorLimitDeg)) {
        xml.raiseError(QStringLiteral("invalid angleMapping attributes"));
        return false;
    }
    mapping->validationCode = attributes.value(
                QStringLiteral("validationCode")).toString();
    bool imageSeen = false;
    bool machineSeen = false;
    while (xml.readNextStartElement()) {
        if (xml.name() == QStringLiteral("image") && !imageSeen) {
            imageSeen = true;
            if (!parseAngleRange(xml.attributes(), &mapping->imageMinDeg,
                                 &mapping->imageMaxDeg,
                                 &mapping->imageCenterDeg)) {
                xml.raiseError(QStringLiteral("invalid image angle range"));
                return false;
            }
            xml.skipCurrentElement();
        } else if (xml.name() == QStringLiteral("machine") && !machineSeen) {
            machineSeen = true;
            if (!parseAngleRange(xml.attributes(), &mapping->machineMinDeg,
                                 &mapping->machineMaxDeg,
                                 &mapping->machineCenterDeg)) {
                xml.raiseError(QStringLiteral("invalid machine angle range"));
                return false;
            }
            xml.skipCurrentElement();
        } else {
            xml.raiseError(QStringLiteral(
                               "unexpected or duplicate angleMapping node"));
            return false;
        }
    }
    if (!imageSeen || !machineSeen) {
        xml.raiseError(QStringLiteral("angleMapping range nodes are incomplete"));
        return false;
    }
    return true;
}

bool parseJsonObjectElement(QXmlStreamReader &xml,
                            QJsonObject *object,
                            const QString &name)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
                xml.readElementText().toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        xml.raiseError(QStringLiteral("%1 JSON is invalid").arg(name));
        return false;
    }
    *object = document.object();
    return true;
}

bool near(double first, double second, double tolerance = 1e-8)
{
    return std::abs(first - second) <= tolerance;
}

bool derivedModelsMatch(const CalibrationModel &model, QString *errorMessage)
{
    if (model.mode == NPointCalibrationMode::NinePointXY)
        return true;
    const CalibrationRotationRange trace = CalibrationSolver().buildAxisTrace(
                model.rotationSamples, model.forward,
                model.rotationRange.minimumSpanDeg,
                model.rotationRange.rmseLimitMm,
                model.rotationRange.maxErrorLimitMm);
    const CalibrationRotationRange &stored = model.rotationRange;
    if (trace.status != stored.status || trace.direction != stored.direction
            || trace.sampleCount != stored.sampleCount
            || trace.coaxial != stored.coaxial
            || trace.validationCode != stored.validationCode
            || !near(trace.trajectoryMinDeg, stored.trajectoryMinDeg)
            || !near(trace.trajectoryMaxDeg, stored.trajectoryMaxDeg)
            || !near(trace.machineMinDeg, stored.machineMinDeg)
            || !near(trace.machineMaxDeg, stored.machineMaxDeg)
            || !near(trace.centerOffsetX, stored.centerOffsetX)
            || !near(trace.centerOffsetY, stored.centerOffsetY)
            || !near(trace.radiusMm, stored.radiusMm)
            || !near(trace.phaseOffsetDeg, stored.phaseOffsetDeg)
            || !near(trace.fitRmseMm, stored.fitRmseMm)
            || !near(trace.maxErrorMm, stored.maxErrorMm)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("axisTrace does not match rotation samples");
        return false;
    }
    if (model.mode != NPointCalibrationMode::TwelvePointPoseMapping)
        return true;
    const CalibrationAngleMapping mapping = CalibrationSolver()
            .buildAngleMapping(model.rotationSamples, model.forward,
                               model.angleMapping.minimumSpanDeg,
                               model.angleMapping.rmseLimitDeg,
                               model.angleMapping.maxErrorLimitDeg);
    const CalibrationAngleMapping &storedMapping = model.angleMapping;
    if (mapping.status != storedMapping.status
            || mapping.direction != storedMapping.direction
            || mapping.sampleCount != storedMapping.sampleCount
            || mapping.validationCode != storedMapping.validationCode
            || !near(mapping.offsetDeg, storedMapping.offsetDeg)
            || !near(mapping.imageMinDeg, storedMapping.imageMinDeg)
            || !near(mapping.imageMaxDeg, storedMapping.imageMaxDeg)
            || !near(mapping.machineMinDeg, storedMapping.machineMinDeg)
            || !near(mapping.machineMaxDeg, storedMapping.machineMaxDeg)
            || !near(mapping.rmseDeg, storedMapping.rmseDeg)
            || !near(mapping.maxErrorDeg, storedMapping.maxErrorDeg)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "angleMapping does not match rotation samples");
        }
        return false;
    }
    return true;
}

} // namespace

QString ProjectXmlCalibrationLoader::formatId() const
{
    return QStringLiteral("project_xml_1_4");
}

bool ProjectXmlCalibrationLoader::canLoad(const QString &filePath) const
{
    if (QFileInfo(filePath).suffix().compare(
            QStringLiteral("xml"), Qt::CaseInsensitive) != 0) {
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement())
        return false;
    return xml.name() == QStringLiteral("calibration")
            || xml.name() == QStringLiteral("CalibrationModel");
}

QByteArray ProjectXmlCalibrationLoader::canonicalPayload(
        const CalibrationModel &model) const
{
    CalibrationModel canonical = model;
    canonical.checksum.clear();
    QByteArray output;
    QXmlStreamWriter xml(&output);
    xml.setAutoFormatting(false);
    writeModel(xml, canonical, false);
    return output;
}

bool ProjectXmlCalibrationLoader::save(const QString &filePath,
                                       const CalibrationModel &input,
                                       QString *errorMessage) const
{
    CalibrationModel model = input;
    if (model.schemaVersion != QStringLiteral("1.4")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "unsupported_calibration_schema: only schemaVersion=1.4 is supported");
        }
        return false;
    }
    QString validationError;
    if (!model.isValid(&validationError)
            || !derivedModelsMatch(model, &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    model.checksum = QString::fromLatin1(QCryptographicHash::hash(
            canonicalPayload(model), QCryptographicHash::Sha256).toHex());

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    writeModel(xml, model, true);
    if (!file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
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
            *errorMessage = QStringLiteral("output model is null");
        return false;
    }
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    QXmlStreamReader xml(&file);
    if (!xml.readNextStartElement()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("missing calibration root node");
        return false;
    }
    const QString rootName = xml.name().toString();
    if (rootName != QStringLiteral("calibration")
            && rootName != QStringLiteral("CalibrationModel")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("not a project calibration XML");
        return false;
    }
    const QXmlStreamAttributes rootAttributes = xml.attributes();
    if (rootAttributes.value(QStringLiteral("schemaVersion"))
            != QStringLiteral("1.4")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "unsupported_calibration_schema: 标定文件版本不受支持，请重新执行标定并生成最新文件");
        }
        return false;
    }
    if (rootName != QStringLiteral("calibration")) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                        "invalid_calibration_structure: XML 1.4 requires the calibration root node");
        }
        return false;
    }

    CalibrationModel parsed;
    parsed.schemaVersion = QStringLiteral("1.4");
    if (!nPointCalibrationModeFromString(
            rootAttributes.value(QStringLiteral("mode")).toString(),
            &parsed.mode)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("invalid calibration mode");
        return false;
    }

    bool identitySeen = false;
    bool methodSeen = false;
    bool modelTypeSeen = false;
    bool conventionSeen = false;
    bool transformSeen = false;
    bool summarySeen = false;
    bool translationSamplesSeen = false;
    bool rotationSamplesSeen = false;
    bool validRegionSeen = false;
    bool safeRegionSeen = false;
    bool axisTraceSeen = false;
    bool angleMappingSeen = false;
    bool qualitySeen = false;
    bool sourceSeen = false;
    bool methodDataSeen = false;
    bool checksumSeen = false;

    while (xml.readNextStartElement()) {
        const QString name = xml.name().toString();
        const QXmlStreamAttributes attributes = xml.attributes();
        if (name == QStringLiteral("identity") && !identitySeen) {
            identitySeen = true;
            parsed.calibrationId = attributes.value(
                        QStringLiteral("calibrationId")).toString();
            parsed.createdAt = attributes.value(
                        QStringLiteral("createdAt")).toString();
            xml.skipCurrentElement();
        } else if (name == QStringLiteral("methodId") && !methodSeen) {
            methodSeen = true;
            parsed.methodId = xml.readElementText();
        } else if (name == QStringLiteral("modelType") && !modelTypeSeen) {
            modelTypeSeen = true;
            parsed.modelType = xml.readElementText();
        } else if (name == QStringLiteral("coordinateConvention")
                   && !conventionSeen) {
            conventionSeen = true;
            parsed.coordinateConvention = xml.readElementText();
        } else if (name == QStringLiteral("transform") && !transformSeen) {
            transformSeen = true;
            if (!calibrationTransformParityFromString(
                    attributes.value(QStringLiteral("parity")).toString(),
                    &parsed.transformParity)
                    || attributes.value(QStringLiteral("model"))
                       != QStringLiteral("affine_2d")) {
                xml.raiseError(QStringLiteral("invalid transform attributes"));
                break;
            }
            bool forwardSeen = false;
            bool inverseSeen = false;
            while (xml.readNextStartElement()) {
                if (xml.name() == QStringLiteral("forward") && !forwardSeen) {
                    forwardSeen = true;
                    if (!parseMatrix(xml.readElementText(), &parsed.forward))
                        xml.raiseError(QStringLiteral("invalid forward transform"));
                } else if (xml.name() == QStringLiteral("inverse")
                           && !inverseSeen) {
                    inverseSeen = true;
                    if (!parseMatrix(xml.readElementText(), &parsed.inverse))
                        xml.raiseError(QStringLiteral("invalid inverse transform"));
                } else {
                    xml.raiseError(QStringLiteral(
                                       "unexpected or duplicate transform node"));
                }
                if (xml.hasError())
                    break;
            }
            if (!forwardSeen || !inverseSeen)
                xml.raiseError(QStringLiteral("transform matrices are incomplete"));
        } else if (name == QStringLiteral("sampleSummary") && !summarySeen) {
            summarySeen = true;
            if (!integerAttribute(attributes,
                                  QStringLiteral("translationCount"),
                                  &parsed.translationSampleCount)
                    || !integerAttribute(attributes,
                                         QStringLiteral("rotationCount"),
                                         &parsed.configuredRotationSampleCount)
                    || !integerAttribute(attributes,
                                         QStringLiteral("totalCount"),
                                         &parsed.totalSampleCount)) {
                xml.raiseError(QStringLiteral("invalid sample summary"));
            }
            xml.skipCurrentElement();
        } else if (name == QStringLiteral("translationSamples")
                   && !translationSamplesSeen) {
            translationSamplesSeen = true;
            parseSamples(xml, true, &parsed.samples);
        } else if (name == QStringLiteral("rotationSamples")
                   && !rotationSamplesSeen) {
            rotationSamplesSeen = true;
            parseSamples(xml, false, &parsed.rotationSamples);
        } else if (name == QStringLiteral("validRegion")
                   && !validRegionSeen) {
            validRegionSeen = true;
            parsed.validRegionCoordinateSystem = attributes.value(
                        QStringLiteral("coordinateSystem")).toString();
            parsed.validRegionType = attributes.value(
                        QStringLiteral("type")).toString();
            parsePointRegion(xml, &parsed.validRegion,
                             QStringLiteral("validRegion"));
        } else if (name == QStringLiteral("safeRegion") && !safeRegionSeen) {
            safeRegionSeen = true;
            parsed.safeRegionCoordinateSystem = attributes.value(
                        QStringLiteral("coordinateSystem")).toString();
            parsed.safeRegionSource = attributes.value(
                        QStringLiteral("source")).toString();
            parsed.safeRegionType = attributes.value(
                        QStringLiteral("type")).toString();
            if (!numberAttribute(attributes, QStringLiteral("marginPx"),
                                 &parsed.safeMarginPx)) {
                xml.raiseError(QStringLiteral("invalid safeRegion margin"));
            } else {
                parsePointRegion(xml, &parsed.safeRegion,
                                 QStringLiteral("safeRegion"));
            }
        } else if (name == QStringLiteral("axisTrace") && !axisTraceSeen) {
            axisTraceSeen = true;
            parseAxisTrace(xml, &parsed.rotationRange);
        } else if (name == QStringLiteral("angleMapping")
                   && !angleMappingSeen) {
            angleMappingSeen = true;
            parseAngleMapping(xml, &parsed.angleMapping);
        } else if (name == QStringLiteral("quality") && !qualitySeen) {
            qualitySeen = true;
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
                    || !numberAttribute(attributes,
                                        QStringLiteral("maxErrorLimit"),
                                        &parsed.quality.maxErrorLimit)
                    || !booleanAttribute(attributes, QStringLiteral("passed"),
                                         &parsed.quality.passed)) {
                xml.raiseError(QStringLiteral("invalid quality fields"));
            }
            xml.skipCurrentElement();
        } else if (name == QStringLiteral("sourceFingerprint")
                   && !sourceSeen) {
            sourceSeen = true;
            parseJsonObjectElement(xml, &parsed.imageBinding,
                                   QStringLiteral("sourceFingerprint"));
        } else if (name == QStringLiteral("methodData") && !methodDataSeen) {
            methodDataSeen = true;
            parseJsonObjectElement(xml, &parsed.methodData,
                                   QStringLiteral("methodData"));
        } else if (name == QStringLiteral("checksum") && !checksumSeen) {
            checksumSeen = true;
            if (attributes.value(QStringLiteral("algorithm"))
                    != QStringLiteral("SHA-256")) {
                xml.raiseError(QStringLiteral("unsupported checksum algorithm"));
            } else {
                parsed.checksum = xml.readElementText().trimmed().toLower();
            }
        } else {
            xml.raiseError(QStringLiteral("unexpected or duplicate node: %1")
                           .arg(name));
        }
        if (xml.hasError())
            break;
    }

    if (xml.hasError()) {
        if (errorMessage)
            *errorMessage = xml.errorString();
        return false;
    }
    if (!identitySeen || !methodSeen || !modelTypeSeen || !conventionSeen
            || !transformSeen || !summarySeen || !translationSamplesSeen
            || !rotationSamplesSeen || !validRegionSeen || !safeRegionSeen
            || !axisTraceSeen || !angleMappingSeen || !qualitySeen
            || !sourceSeen || !methodDataSeen || !checksumSeen) {
        if (errorMessage)
            *errorMessage = QStringLiteral("calibration XML 1.4 nodes are incomplete");
        return false;
    }

    const QString expectedChecksum = QString::fromLatin1(
                QCryptographicHash::hash(canonicalPayload(parsed),
                                         QCryptographicHash::Sha256).toHex());
    if (parsed.checksum.isEmpty() || parsed.checksum != expectedChecksum) {
        if (errorMessage)
            *errorMessage = QStringLiteral("calibration XML SHA-256 verification failed");
        return false;
    }

    QString validationError;
    if (!parsed.isValid(&validationError)
            || !derivedModelsMatch(parsed, &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    *model = parsed;
    return true;
}

bool HikXmlCalibrationLoader::canLoad(const QString &filePath) const
{
    return QFileInfo(filePath).suffix().compare(
                QStringLiteral("xml"), Qt::CaseInsensitive) == 0
            && !ProjectXmlCalibrationLoader().canLoad(filePath);
}

bool HikXmlCalibrationLoader::load(const QString &,
                                   CalibrationModel *,
                                   QString *errorMessage) const
{
    if (errorMessage) {
        *errorMessage = QStringLiteral(
                    "unsupported_format: 尚未取得海康XML样例或官方解析契约");
    }
    return false;
}

bool HikIwcalCalibrationLoader::canLoad(const QString &filePath) const
{
    return QFileInfo(filePath).suffix().compare(
                QStringLiteral("iwcal"), Qt::CaseInsensitive) == 0;
}

bool HikIwcalCalibrationLoader::load(const QString &,
                                     CalibrationModel *,
                                     QString *errorMessage) const
{
    if (errorMessage) {
        *errorMessage = QStringLiteral(
                    "unsupported_format: 尚未取得海康IWCAL解析SDK或格式契约");
    }
    return false;
}
