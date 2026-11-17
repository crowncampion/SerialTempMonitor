#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QStringList>

#include "transport.h"

// 真实串口：对 QSerialPort 的封装。
class SerialManager : public Transport
{
    Q_OBJECT

public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;

    static QStringList availablePorts();
    static QString portDescription(const QString &portName);

    bool open(const SerialConfig &config, QString *errorMessage = nullptr) override;
    void close() override;
    bool isOpen() const override;
    QString portName() const;

    void write(const QByteArray &data) override;

private slots:
    void handleReadyRead();
    void handleError(QSerialPort::SerialPortError error);

private:
    QSerialPort *m_port;
};

#endif // SERIALMANAGER_H
