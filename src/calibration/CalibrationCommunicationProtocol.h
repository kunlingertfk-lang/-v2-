#ifndef CALIBRATION_CALIBRATIONCOMMUNICATIONPROTOCOL_H
#define CALIBRATION_CALIBRATIONCOMMUNICATIONPROTOCOL_H

#include <QJsonObject>
#include <QByteArray>
#include <QHostAddress>
#include <QObject>
#include <QString>

#include <functional>

class QTcpServer;
class QTcpSocket;
class QUdpSocket;

enum class CalibrationCommunicationEvent
{
    Unknown,
    Start,
    Capture,
    End
};

struct CalibrationCommunicationConfig
{
    QString transport = QStringLiteral("none");
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 2000;
    QString startSignal = QStringLiteral("SC");
    QString captureSignal = QStringLiteral("21212");
    QString endSignal = QStringLiteral("EC");
    QString delimiter = QStringLiteral(",");
    QString terminator = QStringLiteral("\\r");
    int xField = 1;
    int yField = 2;
    int angleField = 3;
    QString startOk = QStringLiteral("SC,1");
    QString startNg = QStringLiteral("SC,0");
    QString captureOk = QStringLiteral("21212,1");
    QString captureNg = QStringLiteral("21212,0");
    QString endOk = QStringLiteral("EC,1");
    QString endNg = QStringLiteral("EC,0");
};

struct CalibrationCommunicationMessage
{
    bool valid = false;
    CalibrationCommunicationEvent event = CalibrationCommunicationEvent::Unknown;
    double x = 0.0;
    double y = 0.0;
    double angleDeg = 0.0;
    QString raw;
    QString error;
    QJsonObject logEntry;
};

Q_DECLARE_METATYPE(CalibrationCommunicationMessage)

class CalibrationCommunicationSession : public QObject
{
    Q_OBJECT
public:
    explicit CalibrationCommunicationSession(QObject *parent = nullptr);
    ~CalibrationCommunicationSession() override;

    bool start(const CalibrationCommunicationConfig &config,
               QString *errorMessage = nullptr);
    void stop();
    bool isRunning() const;
    void setMessageHandler(const std::function<bool(
                           CalibrationCommunicationMessage *, QString *)> &handler);

signals:
    void messageReceived(const CalibrationCommunicationMessage &message);
    void stateChanged(const QString &state);

private:
    void attachTcpSocket(QTcpSocket *socket);
    void consumeTcpBytes(const QByteArray &bytes);
    void processMessage(const QString &raw,
                        const QHostAddress &udpSender = QHostAddress(),
                        quint16 udpSenderPort = 0);
    QString responseFor(const CalibrationCommunicationMessage &message) const;

    CalibrationCommunicationConfig m_config;
    QTcpServer *m_tcpServer = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QByteArray m_tcpBuffer;
    bool m_running = false;
    std::function<bool(CalibrationCommunicationMessage *, QString *)> m_messageHandler;
};

class CalibrationCommunicationProtocol
{
public:
    static bool validate(const CalibrationCommunicationConfig &config,
                         QString *errorMessage = nullptr);
    static CalibrationCommunicationMessage parse(
            const QString &raw,
            const CalibrationCommunicationConfig &config);
    static QString decodedTerminator(const QString &text);
};

#endif // CALIBRATION_CALIBRATIONCOMMUNICATIONPROTOCOL_H
