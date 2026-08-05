#include "algorithms/location/CalibrationTransformHalconRunner.h"
#include "calibration/CalibrationCommunicationProtocol.h"
#include "calibration/CalibrationFileLoader.h"
#include "calibration/CalibrationMethodRegistry.h"
#include "calibration/CalibrationSolver.h"
#include "tooladapters/CalibrationTransformAdapter.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFrame>
#include <QHeaderView>
#include <QHostAddress>
#include <QComboBox>
#include <QTableWidget>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTextStream>
#include <QUdpSocket>

#include <cmath>
#include <functional>
#include <memory>

namespace {

bool near(double actual, double expected, double tolerance = 1e-7)
{
    return std::abs(actual - expected) <= tolerance;
}

int fail(const QString &message)
{
    QTextStream(stderr) << "FAIL: " << message << '\n';
    return 1;
}

bool spinUntil(const std::function<bool()> &condition, int timeoutMs = 1500)
{
    QElapsedTimer timer;
    timer.start();
    while (!condition() && timer.elapsed() < timeoutMs)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    return condition();
}

QVector<CalibrationSample> knownSamples()
{
    QVector<CalibrationSample> samples;
    for (int gy = 0; gy < 3; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            CalibrationSample sample;
            sample.column = 100.0 + 170.0 * gx;
            sample.row = 80.0 + 130.0 * gy;
            sample.machineX = 2.0 * sample.column + 0.5 * sample.row + 10.0;
            sample.machineY = -0.25 * sample.column + 3.0 * sample.row - 5.0;
            samples.append(sample);
        }
    }
    return samples;
}

} // namespace

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
    QApplication application(argc, argv);

    const QVector<const ICalibrationMethod *> methods =
            CalibrationMethodRegistry::instance().methods();
    if (methods.size() != 3)
        return fail(QStringLiteral("registry must expose three calibration methods"));
    if (!CalibrationMethodRegistry::instance().method(QStringLiteral("n_point")))
        return fail(QStringLiteral("n_point method is missing"));
    if (CalibrationMethodRegistry::instance().method(QStringLiteral("translation_rotation"))
                ->availability() != CalibrationMethodAvailability::Planned
            || CalibrationMethodRegistry::instance().method(QStringLiteral("calibration_board"))
                ->availability() != CalibrationMethodAvailability::Planned)
        return fail(QStringLiteral("planned calibration method availability is incorrect"));
    const ICalibrationMethod *nPointMethod =
            CalibrationMethodRegistry::instance().method(QStringLiteral("n_point"));
    std::unique_ptr<CalibrationMethodConfigWidget> methodWidget(
                nPointMethod->createConfigWidget(nullptr));
    if (!methodWidget || methodWidget->draft().samples.size() != 9)
        return fail(QStringLiteral("registered method did not provide its own config widget"));
    QTableWidget *pointTable = methodWidget->findChild<QTableWidget *>();
    if (!pointTable || !pointTable->verticalHeader()->isHidden() || !pointTable->isHidden())
        return fail(QStringLiteral("N-point table must be hidden behind the editor without duplicate row headers"));
    int configCardCount = 0;
    for (QFrame *frame : methodWidget->findChildren<QFrame *>()) {
        if (frame->property("panelRole").toString() == QStringLiteral("configCard"))
            ++configCardCount;
    }
    if (configCardCount < 3)
        return fail(QStringLiteral("N-point config must use public configCard semantics"));
    CalibrationProducerSnapshot producer;
    producer.producerId = QStringLiteral("location-1");
    producer.displayName = QStringLiteral("模板定位1");
    producer.valid = true;
    producer.payload = QJsonObject{{QStringLiteral("x"), 321.5},
                                   {QStringLiteral("y"), 210.25},
                                   {QStringLiteral("angle"), 12.0}};
    methodWidget->setProducerSnapshots({producer});
    QComboBox *producerCombo = methodWidget->findChild<QComboBox *>();
    if (!producerCombo || producerCombo->count() < 2)
        return fail(QStringLiteral("N-point producer subscription was not populated"));
    producerCombo->setCurrentIndex(1);
    methodWidget->setLatestPhysicalSample(true, 10.0, 20.0, 30.0);
    QString captureError;
    if (!methodWidget->captureCurrentSample(&captureError))
        return fail(QStringLiteral("communication-linked capture failed: %1").arg(captureError));
    const CalibrationDraft capturedDraft = methodWidget->draft();
    if (capturedDraft.samples.isEmpty()
            || !near(capturedDraft.samples.first().column, 321.5)
            || !near(capturedDraft.samples.first().row, 210.25)
            || !near(capturedDraft.samples.first().machineX, 10.0)
            || !near(capturedDraft.samples.first().machineY, 20.0))
        return fail(QStringLiteral("communication and image pose were not merged into one sample"));

    CalibrationCommunicationConfig protocolConfig;
    const CalibrationCommunicationMessage communication =
            CalibrationCommunicationProtocol::parse(QStringLiteral("21212,1.5,-2.0,30\\r"),
                                                    protocolConfig);
    if (!communication.valid || communication.event != CalibrationCommunicationEvent::Capture
            || !near(communication.x, 1.5) || !near(communication.y, -2.0)
            || !near(communication.angleDeg, 30.0))
        return fail(QStringLiteral("communication field parsing failed"));

    QUdpSocket portProbe;
    if (!portProbe.bind(QHostAddress(QHostAddress::LocalHost), 0))
        return fail(QStringLiteral("cannot allocate UDP smoke port"));
    const quint16 udpPort = portProbe.localPort();
    portProbe.close();
    CalibrationCommunicationConfig udpConfig = protocolConfig;
    udpConfig.transport = QStringLiteral("udp");
    udpConfig.host = QStringLiteral("127.0.0.1");
    udpConfig.port = udpPort;
    CalibrationCommunicationSession udpSession;
    CalibrationCommunicationMessage receivedMessage;
    QObject::connect(&udpSession, &CalibrationCommunicationSession::messageReceived,
                     [&receivedMessage](const CalibrationCommunicationMessage &message) {
        receivedMessage = message;
    });
    QString sessionError;
    if (!udpSession.start(udpConfig, &sessionError))
        return fail(QStringLiteral("UDP session start failed: %1").arg(sessionError));
    QUdpSocket udpClient;
    udpClient.writeDatagram(QByteArray("21212,7,8,9\r"),
                            QHostAddress::LocalHost, udpPort);
    if (!spinUntil([&receivedMessage]() { return receivedMessage.valid; }))
        return fail(QStringLiteral("UDP session did not parse the capture message"));
    if (!spinUntil([&udpClient]() { return udpClient.hasPendingDatagrams(); }))
        return fail(QStringLiteral("UDP session did not send an OK response"));
    QByteArray udpResponse;
    udpResponse.resize(static_cast<int>(udpClient.pendingDatagramSize()));
    udpClient.readDatagram(udpResponse.data(), udpResponse.size());
    if (!udpResponse.startsWith("21212,1"))
        return fail(QStringLiteral("UDP session response is invalid"));
    udpSession.setMessageHandler([](CalibrationCommunicationMessage *, QString *error) {
        if (error)
            *error = QStringLiteral("capture rejected by method");
        return false;
    });
    udpClient.writeDatagram(QByteArray("21212,7,8,9\r"),
                            QHostAddress::LocalHost, udpPort);
    if (!spinUntil([&udpClient]() { return udpClient.hasPendingDatagrams(); }))
        return fail(QStringLiteral("UDP session did not send method-level NG response"));
    udpResponse.resize(static_cast<int>(udpClient.pendingDatagramSize()));
    udpClient.readDatagram(udpResponse.data(), udpResponse.size());
    if (!udpResponse.startsWith("21212,0"))
        return fail(QStringLiteral("UDP method rejection did not produce capture NG"));
    udpSession.stop();

    QTcpServer tcpPortProbe;
    if (!tcpPortProbe.listen(QHostAddress::LocalHost, 0))
        return fail(QStringLiteral("cannot allocate TCP smoke port"));
    const quint16 tcpPort = tcpPortProbe.serverPort();
    tcpPortProbe.close();
    CalibrationCommunicationConfig tcpConfig = protocolConfig;
    tcpConfig.transport = QStringLiteral("tcp_server");
    tcpConfig.host = QStringLiteral("127.0.0.1");
    tcpConfig.port = tcpPort;
    CalibrationCommunicationSession tcpSession;
    CalibrationCommunicationMessage tcpMessage;
    QObject::connect(&tcpSession, &CalibrationCommunicationSession::messageReceived,
                     [&tcpMessage](const CalibrationCommunicationMessage &message) {
        tcpMessage = message;
    });
    if (!tcpSession.start(tcpConfig, &sessionError))
        return fail(QStringLiteral("TCP server start failed: %1").arg(sessionError));
    QTcpSocket tcpClient;
    tcpClient.connectToHost(QHostAddress::LocalHost, tcpPort);
    if (!spinUntil([&tcpClient]() {
            return tcpClient.state() == QAbstractSocket::ConnectedState;
        }))
        return fail(QStringLiteral("TCP smoke client did not connect"));
    tcpClient.write(QByteArray("21212,4,5,6\r"));
    tcpClient.flush();
    if (!spinUntil([&tcpMessage]() { return tcpMessage.valid; }))
        return fail(QStringLiteral("TCP server did not parse the capture message"));
    if (!spinUntil([&tcpClient]() { return tcpClient.bytesAvailable() > 0; }))
        return fail(QStringLiteral("TCP server did not send an OK response"));
    if (!tcpClient.readAll().startsWith("21212,1"))
        return fail(QStringLiteral("TCP server response is invalid"));
    tcpSession.stop();

    CalibrationSolveResult solved = CalibrationSolver().solveNPoint(
                knownSamples(), 1e-5, 1e-5);
    if (!solved.success || !solved.model.quality.passed)
        return fail(QStringLiteral("HALCON N-point solve failed: %1").arg(solved.message));
    if (!near(solved.model.forward[0], 2.0)
            || !near(solved.model.forward[1], 0.5)
            || !near(solved.model.forward[2], 10.0)
            || !near(solved.model.forward[3], -0.25)
            || !near(solved.model.forward[4], 3.0)
            || !near(solved.model.forward[5], -5.0))
        return fail(QStringLiteral("unexpected fitted affine matrix"));

    CalibrationTransformHalconRunner runner;
    const CalibrationTransformHalconResult forward = runner.run(
                solved.model, QStringLiteral("image"), 250.0, 160.0, 0.0);
    if (!forward.success || !near(forward.outputX, 590.0)
            || !near(forward.outputY, 412.5))
        return fail(QStringLiteral("image-to-physical conversion failed: %1").arg(forward.message));
    const CalibrationTransformHalconResult inverse = runner.run(
                solved.model, QStringLiteral("physical"),
                forward.outputX, forward.outputY, forward.outputAngleDeg);
    if (!inverse.success || !near(inverse.outputX, 250.0)
            || !near(inverse.outputY, 160.0))
        return fail(QStringLiteral("physical-to-image conversion failed: %1").arg(inverse.message));

    CalibrationTransformPose calibrationPose;
    calibrationPose.enabled = true;
    calibrationPose.x = 100.0;
    calibrationPose.y = 50.0;
    calibrationPose.joint0AngleDeg = 10.0;
    CalibrationTransformPose runPose;
    runPose.enabled = true;
    runPose.x = 120.0;
    runPose.y = 70.0;
    runPose.joint0AngleDeg = 30.0;
    const CalibrationTransformHalconResult compensatedForward = runner.run(
                solved.model, QStringLiteral("image"), 250.0, 160.0, 12.0,
                calibrationPose, runPose);
    const CalibrationTransformHalconResult compensatedInverse = runner.run(
                solved.model, QStringLiteral("physical"),
                compensatedForward.outputX, compensatedForward.outputY,
                compensatedForward.outputAngleDeg, calibrationPose, runPose);
    if (!compensatedForward.success || !compensatedInverse.success
            || !near(compensatedInverse.outputX, 250.0, 1e-6)
            || !near(compensatedInverse.outputY, 160.0, 1e-6))
        return fail(QStringLiteral("pose-compensated round trip failed"));

    CalibrationTransformPose unsupportedPose;
    unsupportedPose.enabled = true;
    unsupportedPose.joint1AngleDeg = 1.0;
    if (runner.run(solved.model, QStringLiteral("image"), 1.0, 2.0, 0.0,
                   unsupportedPose).status != QStringLiteral("unsupported_joint_pose"))
        return fail(QStringLiteral("non-zero Joint1 must be explicitly unsupported"));

    CalibrationSolveResult degenerate = CalibrationSolver().solveNPoint(
                QVector<CalibrationSample>(3, CalibrationSample()), 0.1, 0.25);
    if (degenerate.success)
        return fail(QStringLiteral("duplicate/degenerate points were accepted"));

    QTemporaryDir temporary;
    if (!temporary.isValid())
        return fail(QStringLiteral("temporary directory creation failed"));
    const QString filePath = QDir(temporary.path()).filePath(QStringLiteral("calibration.xml"));
    ProjectXmlCalibrationLoader loader;
    QString error;
    if (!loader.save(filePath, solved.model, &error))
        return fail(QStringLiteral("native XML save failed: %1").arg(error));
    CalibrationModel loaded;
    if (!loader.load(filePath, &loaded, &error))
        return fail(QStringLiteral("native XML reload failed: %1").arg(error));
    if (loaded.calibrationId != solved.model.calibrationId
            || !near(loaded.forward[0], solved.model.forward[0]))
        return fail(QStringLiteral("native XML round-trip changed the model"));

    const auto boundInput = [](const QString &producerId, const QString &outputKey) {
        return QJsonObject{{QStringLiteral("mode"), QStringLiteral("binding")},
                           {QStringLiteral("producerId"), producerId},
                           {QStringLiteral("outputKey"), outputKey}};
    };
    const auto constantInput = [](double value) {
        return QJsonObject{{QStringLiteral("mode"), QStringLiteral("constant")},
                           {QStringLiteral("value"), value}};
    };
    ToolRequest adapterRequest;
    adapterRequest.config.toolId = QStringLiteral("calibration-transform-smoke");
    adapterRequest.config.toolType = ToolType::CalibrationTransform;
    adapterRequest.config.category = ToolCategory::Location;
    QJsonObject adapterParams{
        {QStringLiteral("coordinateType"), QStringLiteral("image")},
        {QStringLiteral("activeCalibrationFile"), filePath},
        {QStringLiteral("inputX"), boundInput(QStringLiteral("pose-source"), QStringLiteral("x"))},
        {QStringLiteral("inputY"), boundInput(QStringLiteral("pose-source"), QStringLiteral("y"))},
        {QStringLiteral("inputAngle"), constantInput(0.0)},
        {QStringLiteral("calibrationPose"), QJsonObject{{QStringLiteral("enabled"), false}}},
        {QStringLiteral("runPose"), QJsonObject{{QStringLiteral("enabled"), false}}}
    };
    adapterRequest.config.params.insert(QStringLiteral("calibrationTransform"), adapterParams);
    adapterRequest.runtimeContext = QJsonObject{
        {QStringLiteral("frameId"), QStringLiteral("frame-1")},
        {QStringLiteral("toolResultsById"), QJsonObject{
             {QStringLiteral("pose-source"), QJsonObject{
                  {QStringLiteral("success"), true},
                  {QStringLiteral("payload"), QJsonObject{
                       {QStringLiteral("x"), 250.0},
                       {QStringLiteral("y"), 160.0},
                       {QStringLiteral("frameId"), QStringLiteral("frame-1")}}}}}}}
    };
    CalibrationTransformAdapter adapter;
    const ToolResult adapterResult = adapter.run(adapterRequest);
    if (!adapterResult.success
            || !near(adapterResult.payload.value(QStringLiteral("machineX")).toDouble(), 590.0))
        return fail(QStringLiteral("adapter producer binding conversion failed: %1")
                    .arg(adapterResult.message));
    adapterParams.insert(QStringLiteral("inputY"),
                         boundInput(QStringLiteral("another-source"), QStringLiteral("y")));
    adapterRequest.config.params.insert(QStringLiteral("calibrationTransform"), adapterParams);
    if (adapter.run(adapterRequest).status != QStringLiteral("mixed_binding_producers"))
        return fail(QStringLiteral("cross-producer mixed binding was not rejected"));

    CalibrationModel futureModel = solved.model;
    futureModel.methodId = QStringLiteral("future_method");
    const QString futurePath = QDir(temporary.path()).filePath(QStringLiteral("future.xml"));
    if (!loader.save(futurePath, futureModel, &error))
        return fail(QStringLiteral("future-method summary save failed: %1").arg(error));
    CalibrationModel futureLoaded;
    if (!loader.load(futurePath, &futureLoaded, &error)
            || futureLoaded.summaryJson().value(QStringLiteral("methodId")).toString()
               != QStringLiteral("future_method"))
        return fail(QStringLiteral("unknown method public summary could not be loaded: %1").arg(error));
    if (runner.run(futureLoaded, QStringLiteral("image"), 1.0, 2.0, 0.0).status
            != QStringLiteral("unsupported_method"))
        return fail(QStringLiteral("unknown method execution was not rejected"));

    const QString unknownSchemaPath = QDir(temporary.path()).filePath(QStringLiteral("schema2.xml"));
    if (!QFile::copy(filePath, unknownSchemaPath))
        return fail(QStringLiteral("cannot prepare unknown schema test"));
    QFile schemaFile(unknownSchemaPath);
    if (!schemaFile.open(QIODevice::ReadWrite))
        return fail(QStringLiteral("cannot open unknown schema test file"));
    QByteArray schemaBytes = schemaFile.readAll();
    schemaBytes.replace("schemaVersion=\"1.0\"", "schemaVersion=\"2.0\"");
    schemaFile.resize(0);
    schemaFile.write(schemaBytes);
    schemaFile.close();
    CalibrationModel schemaModel;
    if (loader.load(unknownSchemaPath, &schemaModel, &error)
            || !error.contains(QStringLiteral("schema version")))
        return fail(QStringLiteral("unknown schema major version was not rejected: %1").arg(error));

    QFile file(filePath);
    if (!file.open(QIODevice::ReadWrite))
        return fail(QStringLiteral("cannot open XML for checksum corruption test"));
    QByteArray bytes = file.readAll();
    const int checksumTag = bytes.indexOf("<checksum ");
    const int checksumText = checksumTag < 0 ? -1 : bytes.indexOf('>', checksumTag) + 1;
    if (checksumText <= 0 || checksumText >= bytes.size())
        return fail(QStringLiteral("checksum element was not written"));
    bytes[checksumText] = bytes.at(checksumText) == '0' ? '1' : '0';
    file.resize(0);
    file.write(bytes);
    file.close();
    CalibrationModel corrupted;
    if (loader.load(filePath, &corrupted, &error)
            || !error.contains(QStringLiteral("校验")))
        return fail(QStringLiteral("checksum corruption was not rejected: %1").arg(error));

    QTextStream(stdout) << "PASS: calibration core smoke\n";
    return 0;
}
