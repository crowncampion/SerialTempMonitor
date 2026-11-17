#include "virtualserialport.h"

#include "framecodec.h"

#include <QRandomGenerator>
#include <cmath>

VirtualSerialPort::VirtualSerialPort(QObject *parent)
    : Transport(parent)
{
    m_timer.setInterval(500);
    connect(&m_timer, &QTimer::timeout, this, &VirtualSerialPort::tick);
}

bool VirtualSerialPort::open(const SerialConfig &config, QString *errorMessage)
{
    Q_UNUSED(config)
    Q_UNUSED(errorMessage)

    if (m_open)
        close();

    m_phase = 0.0;
    m_open = true;
    m_timer.start();
    emit stateChanged(true);
    return true;
}

void VirtualSerialPort::close()
{
    if (!m_open)
        return;

    m_timer.stop();
    m_open = false;
    emit stateChanged(false);
}

bool VirtualSerialPort::isOpen() const
{
    return m_open;
}

void VirtualSerialPort::write(const QByteArray &data)
{
    // 虚拟串口没有真实对端：把写出的数据立刻当作收到的数据送回，
    // 相当于模拟了一根把 TX 和 RX 短接的回环线。
    if (m_open && !data.isEmpty())
        emit dataReceived(data);
}

void VirtualSerialPort::tick()
{
    m_phase += 0.12;

    // 和真实设备一样：温度缓慢起伏 + 少量噪声。
    const double noise = (QRandomGenerator::global()->generateDouble() - 0.5) * 0.6;
    const double temperature = 25.0 + 6.0 * std::sin(m_phase) + noise;
    const double humidity = 50.0 + 15.0 * std::sin(m_phase * 0.5) + noise * 2.0;

    emit dataReceived(FrameCodec::encode(temperature, humidity));
}
