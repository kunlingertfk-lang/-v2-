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

/// 快速标定通信参数：描述传输方式、报文分隔规则、字段位置及各事件应答文本。
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

/// 单条入站标定报文的解析与业务处理结果，同时携带可持久化的通信日志。
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

/// 管理一次快速标定通信会话，并把 TCP/UDP 字节流统一交给标定报文协议处理。
class CalibrationCommunicationSession : public QObject
{
    Q_OBJECT
public:
    explicit CalibrationCommunicationSession(QObject *parent = nullptr);
    ~CalibrationCommunicationSession() override;

    /// 校验配置并启动无设备、TCP 客户端、TCP 服务端或 UDP 会话。
    bool start(const CalibrationCommunicationConfig &config,
               QString *errorMessage = nullptr);
    /// 关闭当前 socket/server、清空 TCP 半包缓存并切换到 stopped 状态。
    void stop();
    /// 返回会话是否已启动；TCP 客户端断开后会恢复为 false。
    bool isRunning() const;
    /// 注册同步业务处理器；其返回值参与所有有效事件的最终 OK/NG 判定。
    void setMessageHandler(const std::function<bool(
                           CalibrationCommunicationMessage *, QString *)> &handler);

signals:
    /// 报文解析、业务处理和应答选择完成后发出，供向导更新日志与界面。
    void messageReceived(const CalibrationCommunicationMessage &message);
    /// 连接、监听、断开或错误状态变化通知。
    void stateChanged(const QString &state);

private:
    /// 绑定 TCP socket 的连接、收包、断开和错误回调。
    void attachTcpSocket(QTcpSocket *socket);
    /// 累积 TCP 半包，并按解码后的终止符切出完整报文。
    void consumeTcpBytes(const QByteArray &bytes);
    /// 执行“解析 -> 业务处理 -> 选择应答 -> 发信号 -> 尝试回包”的统一链路。
    void processMessage(const QString &raw,
                        const QHostAddress &udpSender = QHostAddress(),
                        quint16 udpSenderPort = 0);
    /// 按事件类型及最终 valid 状态选择配置的 OK/NG 应答文本。
    QString responseFor(const CalibrationCommunicationMessage &message) const;

    CalibrationCommunicationConfig m_config;
    QTcpServer *m_tcpServer = nullptr;
    QTcpSocket *m_tcpSocket = nullptr;
    QUdpSocket *m_udpSocket = nullptr;
    QByteArray m_tcpBuffer;
    bool m_running = false;
    std::function<bool(CalibrationCommunicationMessage *, QString *)> m_messageHandler;
};

/// 无状态的标定通信协议工具，负责配置校验、转义字符解码和单报文解析。
class CalibrationCommunicationProtocol
{
public:
    /// 校验信号、字段序号、分隔符、应答文本和端口等协议约束。
    static bool validate(const CalibrationCommunicationConfig &config,
                         QString *errorMessage = nullptr);
    /// 把原始文本解析为 Start/Capture/End；Capture 同时提取有限的 X/Y/Angle。
    static CalibrationCommunicationMessage parse(
            const QString &raw,
            const CalibrationCommunicationConfig &config);
    /// 将配置文本中的 \r、\n、\t 转为真实终止字符。
    static QString decodedTerminator(const QString &text);
};

#endif // CALIBRATION_CALIBRATIONCOMMUNICATIONPROTOCOL_H
