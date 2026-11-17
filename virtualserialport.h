#ifndef VIRTUALSERIALPORT_H
#define VIRTUALSERIALPORT_H

#include <QTimer>

#include "transport.h"

// 程序内部的“虚拟串口”：
// 和真实串口一样需要 open()/close()，通过 dataReceived 异步送数据。
// 它不占用任何系统 COM 口，也不需要 com0com 之类的驱动。
class VirtualSerialPort : public Transport
{
    Q_OBJECT

public:
    explicit VirtualSerialPort(QObject *parent = nullptr);

    bool open(const SerialConfig &config, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    void write(const QByteArray &data) override;

private slots:
    void tick();

private:
    QTimer m_timer;
    bool m_open = false;
    double m_phase = 0.0;
};

#endif // VIRTUALSERIALPORT_H
