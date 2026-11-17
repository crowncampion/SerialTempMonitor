#include "serialmanager.h"

#include <QSerialPortInfo>

namespace {
// 关闭前等待写缓冲清空的上限：只在确实有积压时才等待，避免界面卡顿。
constexpr int kWriteDrainTimeoutMs = 200;
}

SerialManager::SerialManager(QObject *parent)
    : Transport(parent)
    , m_port(new QSerialPort(this))
{
    connect(m_port, &QSerialPort::readyRead, this, &SerialManager::handleReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, &SerialManager::handleError);
}

SerialManager::~SerialManager()
{
    if (!m_port->isOpen())
        return;

    // 与 close() 相同的防御：析构前尽量把未发完的数据交给驱动。
    if (m_port->bytesToWrite() > 0)
        m_port->waitForBytesWritten(kWriteDrainTimeoutMs);

    m_port->close();
}

QStringList SerialManager::availablePorts()
{
    QStringList names;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports)
        names.append(info.portName());
    return names;
}

QString SerialManager::portDescription(const QString &portName)
{
    const auto ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : ports) {
        if (info.portName() == portName) {
            if (!info.description().isEmpty())
                return info.description();
            return info.manufacturer();
        }
    }
    return QString();
}

bool SerialManager::open(const SerialConfig &config, QString *errorMessage)
{
    if (m_port->isOpen())
        close();

    m_port->setPortName(config.portName);
    m_port->setBaudRate(config.baudRate);
    m_port->setDataBits(config.dataBits);
    m_port->setParity(config.parity);
    m_port->setStopBits(config.stopBits);
    m_port->setFlowControl(config.flowControl);

    if (!m_port->open(QIODevice::ReadWrite)) {
        const QString message = m_port->errorString();
        if (errorMessage)
            *errorMessage = message;
        emit errorOccurred(message);
        return false;
    }

    m_port->clear();
    emit stateChanged(true);
    return true;
}

void SerialManager::close()
{
    if (!m_port->isOpen())
        return;

    // 防御性：如果还有没交给驱动的数据，最多等 200ms 再关闭，
    // 避免“发送后立刻关闭”丢掉最后一帧；没有积压时零额外开销。
    if (m_port->bytesToWrite() > 0)
        m_port->waitForBytesWritten(kWriteDrainTimeoutMs);

    m_port->close();
    emit stateChanged(false);
}

bool SerialManager::isOpen() const
{
    return m_port->isOpen();
}

QString SerialManager::portName() const
{
    return m_port->portName();
}

void SerialManager::write(const QByteArray &data)
{
    if (m_port->isOpen())
        m_port->write(data);
}

void SerialManager::handleReadyRead()
{
    const QByteArray data = m_port->readAll();
    if (!data.isEmpty())
        emit dataReceived(data);
}

void SerialManager::handleError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError)
        return;

    emit errorOccurred(m_port->errorString());

    // 设备被拔掉时串口已经不可用，主动关闭，让界面恢复初始状态。
    if (error == QSerialPort::ResourceError && m_port->isOpen())
        close();
}
