#include "calibration/CalibrationCommunicationProtocol.h"

#include <QDateTime>
#include <QHostAddress>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>

#include <cmath>

namespace {

QString eventName(CalibrationCommunicationEvent event)
{
    switch (event) {
    case CalibrationCommunicationEvent::Start: return QStringLiteral("start");
    case CalibrationCommunicationEvent::Capture: return QStringLiteral("capture");
    case CalibrationCommunicationEvent::End: return QStringLiteral("end");
    default: return QStringLiteral("unknown");
    }
}

CalibrationCommunicationMessage failed(const QString &raw, const QString &error)
{
    CalibrationCommunicationMessage message;
    message.raw = raw;
    message.error = error;
    message.logEntry = QJsonObject{
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
        {QStringLiteral("direction"), QStringLiteral("input")},
        {QStringLiteral("raw"), raw},
        {QStringLiteral("valid"), false},
        {QStringLiteral("error"), error}
    };
    return message;
}

} // namespace

QString CalibrationCommunicationProtocol::decodedTerminator(const QString &text)
{
    QString decoded = text;
    decoded.replace(QStringLiteral("\\r"), QStringLiteral("\r"));
    decoded.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    decoded.replace(QStringLiteral("\\t"), QStringLiteral("\t"));
    return decoded;
}

bool CalibrationCommunicationProtocol::validate(
        const CalibrationCommunicationConfig &config,
        QString *errorMessage)
{
    const auto fail = [errorMessage](const QString &text) {
        if (errorMessage)
            *errorMessage = text;
        return false;
    };
    if (config.startSignal.trimmed().isEmpty()
            || config.captureSignal.trimmed().isEmpty()
            || config.endSignal.trimmed().isEmpty())
        return fail(QStringLiteral("开始、标定和结束信号均不能为空"));
    if (config.delimiter.isEmpty())
        return fail(QStringLiteral("分隔符不能为空"));
    if (config.xField < 1 || config.yField < 1 || config.angleField < 1)
        return fail(QStringLiteral("坐标字段序号必须从1开始"));
    if (config.xField == config.yField || config.xField == config.angleField
            || config.yField == config.angleField)
        return fail(QStringLiteral("X、Y、Angle字段不能重复"));
    if (config.startOk.trimmed().isEmpty() || config.startNg.trimmed().isEmpty()
            || config.captureOk.trimmed().isEmpty() || config.captureNg.trimmed().isEmpty()
            || config.endOk.trimmed().isEmpty() || config.endNg.trimmed().isEmpty())
        return fail(QStringLiteral("开始、标定和结束信号的OK/NG输出均不能为空"));
    if (config.transport != QStringLiteral("none") && config.port == 0)
        return fail(QStringLiteral("通信端口必须在1到65535之间"));
    return true;
}

CalibrationCommunicationSession::CalibrationCommunicationSession(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<CalibrationCommunicationMessage>("CalibrationCommunicationMessage");
}

CalibrationCommunicationSession::~CalibrationCommunicationSession()
{
    stop();
}

bool CalibrationCommunicationSession::start(
        const CalibrationCommunicationConfig &config,
        QString *errorMessage)
{
    stop();
    if (!CalibrationCommunicationProtocol::validate(config, errorMessage))
        return false;
    m_config = config;
    if (config.transport == QStringLiteral("none")) {
        m_running = true;
        emit stateChanged(QStringLiteral("manual"));
        return true;
    }
    if (config.transport == QStringLiteral("tcp_client")) {
        QTcpSocket *socket = new QTcpSocket(this);
        attachTcpSocket(socket);
        socket->connectToHost(config.host, config.port);
        m_running = true;
        emit stateChanged(QStringLiteral("connecting"));
        return true;
    }
    if (config.transport == QStringLiteral("tcp_server")) {
        m_tcpServer = new QTcpServer(this);
        const QHostAddress address = config.host.trimmed().isEmpty()
                ? QHostAddress::Any : QHostAddress(config.host);
        if (!m_tcpServer->listen(address, config.port)) {
            if (errorMessage)
                *errorMessage = m_tcpServer->errorString();
            stop();
            return false;
        }
        connect(m_tcpServer, &QTcpServer::newConnection, this, [this]() {
            if (m_tcpSocket)
                m_tcpSocket->deleteLater();
            attachTcpSocket(m_tcpServer->nextPendingConnection());
            emit stateChanged(QStringLiteral("connected"));
        });
        m_running = true;
        emit stateChanged(QStringLiteral("listening"));
        return true;
    }
    if (config.transport == QStringLiteral("udp")) {
        m_udpSocket = new QUdpSocket(this);
        const QHostAddress address = config.host.trimmed().isEmpty()
                ? QHostAddress::Any : QHostAddress(config.host);
        if (!m_udpSocket->bind(address, config.port,
                              QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
            if (errorMessage)
                *errorMessage = m_udpSocket->errorString();
            stop();
            return false;
        }
        connect(m_udpSocket, &QUdpSocket::readyRead, this, [this]() {
            while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
                QByteArray datagram;
                datagram.resize(static_cast<int>(m_udpSocket->pendingDatagramSize()));
                QHostAddress sender;
                quint16 senderPort = 0;
                m_udpSocket->readDatagram(datagram.data(), datagram.size(),
                                          &sender, &senderPort);
                processMessage(QString::fromUtf8(datagram), sender, senderPort);
            }
        });
        m_running = true;
        emit stateChanged(QStringLiteral("listening"));
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("不支持的通信方式: %1").arg(config.transport);
    return false;
}

void CalibrationCommunicationSession::stop()
{
    m_running = false;
    m_tcpBuffer.clear();
    if (m_tcpSocket) {
        m_tcpSocket->disconnect(this);
        m_tcpSocket->close();
        m_tcpSocket->deleteLater();
        m_tcpSocket = nullptr;
    }
    if (m_tcpServer) {
        m_tcpServer->close();
        m_tcpServer->deleteLater();
        m_tcpServer = nullptr;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        m_udpSocket->deleteLater();
        m_udpSocket = nullptr;
    }
    emit stateChanged(QStringLiteral("stopped"));
}

bool CalibrationCommunicationSession::isRunning() const
{
    return m_running;
}

void CalibrationCommunicationSession::setMessageHandler(
        const std::function<bool(CalibrationCommunicationMessage *, QString *)> &handler)
{
    m_messageHandler = handler;
}

void CalibrationCommunicationSession::attachTcpSocket(QTcpSocket *socket)
{
    m_tcpSocket = socket;
    if (!socket)
        return;
    connect(socket, &QTcpSocket::connected, this, [this]() {
        emit stateChanged(QStringLiteral("connected"));
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        consumeTcpBytes(socket->readAll());
    });
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        if (!m_tcpServer)
            m_running = false;
        emit stateChanged(QStringLiteral("disconnected"));
    });
    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, [this, socket](QAbstractSocket::SocketError) {
        if (!m_tcpServer)
            m_running = false;
        emit stateChanged(QStringLiteral("error: %1").arg(socket->errorString()));
    });
}

void CalibrationCommunicationSession::consumeTcpBytes(const QByteArray &bytes)
{
    m_tcpBuffer.append(bytes);
    const QByteArray terminator = CalibrationCommunicationProtocol::decodedTerminator(
                m_config.terminator).toUtf8();
    if (terminator.isEmpty()) {
        const QByteArray message = m_tcpBuffer;
        m_tcpBuffer.clear();
        processMessage(QString::fromUtf8(message));
        return;
    }
    int endIndex = -1;
    while ((endIndex = m_tcpBuffer.indexOf(terminator)) >= 0) {
        const int length = endIndex + terminator.size();
        const QByteArray message = m_tcpBuffer.left(length);
        m_tcpBuffer.remove(0, length);
        processMessage(QString::fromUtf8(message));
    }
}

QString CalibrationCommunicationSession::responseFor(
        const CalibrationCommunicationMessage &message) const
{
    const bool ok = message.valid;
    switch (message.event) {
    case CalibrationCommunicationEvent::Start:
        return ok ? m_config.startOk : m_config.startNg;
    case CalibrationCommunicationEvent::Capture:
        return ok ? m_config.captureOk : m_config.captureNg;
    case CalibrationCommunicationEvent::End:
        return ok ? m_config.endOk : m_config.endNg;
    default:
        return m_config.captureNg;
    }
}

void CalibrationCommunicationSession::processMessage(
        const QString &raw,
        const QHostAddress &udpSender,
        quint16 udpSenderPort)
{
    CalibrationCommunicationMessage message =
            CalibrationCommunicationProtocol::parse(raw, m_config);
    if (message.valid && m_messageHandler) {
        QString handlingError;
        if (!m_messageHandler(&message, &handlingError)) {
            message.valid = false;
            message.error = handlingError.isEmpty()
                    ? QStringLiteral("标定流程未接受该信号") : handlingError;
            message.logEntry.insert(QStringLiteral("valid"), false);
            message.logEntry.insert(QStringLiteral("error"), message.error);
        }
    }
    const QString responseText = responseFor(message);
    message.logEntry.insert(QStringLiteral("response"), responseText);
    message.logEntry.insert(QStringLiteral("responseTimestamp"),
                            QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    emit messageReceived(message);
    const QByteArray response = (responseText
                                 + CalibrationCommunicationProtocol::decodedTerminator(
                                     m_config.terminator)).toUtf8();
    if (m_udpSocket && !udpSender.isNull() && udpSenderPort != 0)
        m_udpSocket->writeDatagram(response, udpSender, udpSenderPort);
    else if (m_tcpSocket && m_tcpSocket->state() == QAbstractSocket::ConnectedState)
        m_tcpSocket->write(response);
}

CalibrationCommunicationMessage CalibrationCommunicationProtocol::parse(
        const QString &raw,
        const CalibrationCommunicationConfig &config)
{
    QString configError;
    if (!validate(config, &configError))
        return failed(raw, configError);

    QString normalized = raw;
    const QString terminator = decodedTerminator(config.terminator);
    if (!config.terminator.isEmpty() && normalized.endsWith(config.terminator))
        normalized.chop(config.terminator.size());
    else if (!terminator.isEmpty() && normalized.endsWith(terminator))
        normalized.chop(terminator.size());
    const QStringList fields = normalized.split(config.delimiter, Qt::KeepEmptyParts);
    if (fields.isEmpty())
        return failed(raw, QStringLiteral("空报文"));

    CalibrationCommunicationMessage message;
    message.raw = raw;
    const QString command = fields.first().trimmed();
    if (command == config.startSignal.trimmed()) {
        message.event = CalibrationCommunicationEvent::Start;
    } else if (command == config.endSignal.trimmed()) {
        message.event = CalibrationCommunicationEvent::End;
    } else if (command == config.captureSignal.trimmed()) {
        message.event = CalibrationCommunicationEvent::Capture;
        const int maximumIndex = qMax(config.xField, qMax(config.yField, config.angleField));
        if (fields.size() <= maximumIndex)
            return failed(raw, QStringLiteral("标定报文缺少坐标字段"));
        bool xOk = false;
        bool yOk = false;
        bool angleOk = false;
        message.x = fields.at(config.xField).trimmed().toDouble(&xOk);
        message.y = fields.at(config.yField).trimmed().toDouble(&yOk);
        message.angleDeg = fields.at(config.angleField).trimmed().toDouble(&angleOk);
        if (!xOk || !yOk || !angleOk || !std::isfinite(message.x)
                || !std::isfinite(message.y) || !std::isfinite(message.angleDeg))
            return failed(raw, QStringLiteral("坐标字段不是有限数字"));
    } else {
        return failed(raw, QStringLiteral("未知通信信号"));
    }
    message.valid = true;
    message.logEntry = QJsonObject{
        {QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODateWithMs)},
        {QStringLiteral("direction"), QStringLiteral("input")},
        {QStringLiteral("raw"), raw},
        {QStringLiteral("valid"), true},
        {QStringLiteral("event"), eventName(message.event)},
        {QStringLiteral("x"), message.x},
        {QStringLiteral("y"), message.y},
        {QStringLiteral("angleDeg"), message.angleDeg}
    };
    return message;
}
