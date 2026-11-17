#ifndef TRANSPORT_H
#define TRANSPORT_H

#include <QByteArray>
#include <QObject>
#include <QSerialPort>
#include <QString>

// 串口参数，界面选好后打包传给传输层。
struct SerialConfig
{
    QString portName;
    qint32 baudRate = 115200;
    QSerialPort::DataBits dataBits = QSerialPort::Data8;
    QSerialPort::Parity parity = QSerialPort::NoParity;
    QSerialPort::StopBits stopBits = QSerialPort::OneStop;
    QSerialPort::FlowControl flowControl = QSerialPort::NoFlowControl;
};

// 数据源统一接口：
//   - SerialManager      使用真实 QSerialPort
//   - VirtualSerialPort  在程序内部模拟一个串口
// 两者对界面的行为完全一致：打开/关闭、状态变化、收到数据。
class Transport : public QObject
{
    Q_OBJECT

public:
    explicit Transport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }
    ~Transport() override = default;

    virtual bool open(const SerialConfig &config, QString *errorMessage = nullptr) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual void write(const QByteArray &data) = 0;

signals:
    void dataReceived(const QByteArray &data);
    void stateChanged(bool opened);
    void errorOccurred(const QString &message);
};

#endif // TRANSPORT_H
