#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "framecodec.h"

#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QRandomGenerator>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardPaths>
#include <QStringConverter>
#include <QTableWidgetItem>
#include <QTextStream>

namespace {
// 超过这个时间没有新样本，就在状态栏提示“数据已过期”。
constexpr int kStaleThresholdMs = 5000;

// 按 data() 里的值设置下拉框选中项。
void setComboData(QComboBox *combo, const QVariant &value)
{
    const int index = combo->findData(value);
    if (index >= 0)
        combo->setCurrentIndex(index);
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 温度值用大号字突出显示。
    QFont valueFont = ui->tempValueLabel->font();
    valueFont.setPointSize(22);
    valueFont.setBold(true);
    ui->tempValueLabel->setFont(valueFont);

    QFont humidityFont = ui->humidityValueLabel->font();
    humidityFont.setPointSize(14);
    ui->humidityValueLabel->setFont(humidityFont);

    ui->sampleTable->horizontalHeader()->setStretchLastSection(true);
    ui->sampleTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    setupCombos();
    setupPlot();
    setupConnections();

    // 初始化和恢复配置期间不要触发“改一项就保存”，
    // 否则还没读完的值会被默认值覆盖。
    m_loadingSettings = true;
    refreshPorts();
    loadSettings();
    m_loadingSettings = false;

    m_staleLabel = new QLabel(this);
    m_staleLabel->setToolTip(QStringLiteral("最后一条样本的接收时间"));
    statusBar()->addPermanentWidget(m_staleLabel);

    connect(&m_staleTimer, &QTimer::timeout, this, &MainWindow::updateDataFreshness);
    m_staleTimer.start(1000);
    updateDataFreshness();

    ui->alarmLabel->setText(QStringLiteral("正常"));
    ui->alarmLabel->setStyleSheet(QStringLiteral("color:#2e7d32; font-weight:bold;"));
    ui->recordPathLabel->setText(QStringLiteral("未记录"));

    statusBar()->showMessage(QStringLiteral("就绪"));
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::startSimulation()
{
    if (isTransportOpen())
        closeTransport();

    const int index = ui->portCombo->findData(QStringLiteral("__virtual__"));
    if (index >= 0)
        ui->portCombo->setCurrentIndex(index);

    QString error;
    if (!openSelectedTransport(&error))
        qWarning() << "无法打开虚拟串口:" << error;
}

void MainWindow::setupCombos()
{
    const int baudRates[] = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
    for (int baud : baudRates)
        ui->baudCombo->addItem(QString::number(baud), baud);
    setComboData(ui->baudCombo, 115200);

    ui->dataBitsCombo->addItem(QStringLiteral("5"), static_cast<int>(QSerialPort::Data5));
    ui->dataBitsCombo->addItem(QStringLiteral("6"), static_cast<int>(QSerialPort::Data6));
    ui->dataBitsCombo->addItem(QStringLiteral("7"), static_cast<int>(QSerialPort::Data7));
    ui->dataBitsCombo->addItem(QStringLiteral("8"), static_cast<int>(QSerialPort::Data8));
    setComboData(ui->dataBitsCombo, static_cast<int>(QSerialPort::Data8));

    ui->parityCombo->addItem(QStringLiteral("无"), static_cast<int>(QSerialPort::NoParity));
    ui->parityCombo->addItem(QStringLiteral("偶校验"), static_cast<int>(QSerialPort::EvenParity));
    ui->parityCombo->addItem(QStringLiteral("奇校验"), static_cast<int>(QSerialPort::OddParity));
    ui->parityCombo->addItem(QStringLiteral("标记"), static_cast<int>(QSerialPort::MarkParity));
    ui->parityCombo->addItem(QStringLiteral("空格"), static_cast<int>(QSerialPort::SpaceParity));
    setComboData(ui->parityCombo, static_cast<int>(QSerialPort::NoParity));

    ui->stopBitsCombo->addItem(QStringLiteral("1"), static_cast<int>(QSerialPort::OneStop));
    ui->stopBitsCombo->addItem(QStringLiteral("1.5"), static_cast<int>(QSerialPort::OneAndHalfStop));
    ui->stopBitsCombo->addItem(QStringLiteral("2"), static_cast<int>(QSerialPort::TwoStop));
    setComboData(ui->stopBitsCombo, static_cast<int>(QSerialPort::OneStop));

    const int show_range[] = {30, 60, 120, 300};
    for (int seconds : show_range)
        ui->windowCombo->addItem(QStringLiteral("%1 秒").arg(seconds), seconds);
    setComboData(ui->windowCombo, 60);
}

void MainWindow::setupPlot()
{
    ui->plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);
    ui->plot->legend->setVisible(true);

    ui->plot->xAxis->setLabel(QStringLiteral("时间"));
    ui->plot->yAxis->setLabel(QStringLiteral("温度 (℃)"));
    ui->plot->yAxis2->setLabel(QStringLiteral("湿度 (%)"));
    ui->plot->yAxis2->setVisible(true);
    ui->plot->xAxis2->setVisible(false);

    ui->plot->yAxis->setRange(0, 50);
    ui->plot->yAxis2->setRange(0, 100);

    // QCustomPlot 默认只让底部 xAxis 和左侧 yAxis 参与拖动/缩放；
    // 湿度曲线挂在右侧 yAxis2 上，必须把它也加进去，
    // 否则拖动或缩放时蓝线不会跟着动。
    const QList<QCPAxis *> horizontalAxes = {ui->plot->xAxis};
    const QList<QCPAxis *> verticalAxes = {ui->plot->yAxis, ui->plot->yAxis2};
    ui->plot->axisRect()->setRangeDragAxes(horizontalAxes, verticalAxes);
    ui->plot->axisRect()->setRangeZoomAxes(horizontalAxes, verticalAxes);

    QSharedPointer<QCPAxisTickerDateTime> timeTicker(new QCPAxisTickerDateTime);
    timeTicker->setDateTimeFormat(QStringLiteral("hh:mm:ss"));
    ui->plot->xAxis->setTicker(timeTicker);

    // 温度用左轴（红色），湿度用右轴（蓝色）。
    ui->plot->addGraph();
    QPen temperaturePen(QColor(214, 69, 65));
    temperaturePen.setWidth(2);
    ui->plot->graph(0)->setPen(temperaturePen);
    ui->plot->graph(0)->setName(QStringLiteral("温度"));

    ui->plot->addGraph(ui->plot->xAxis, ui->plot->yAxis2);
    QPen humidityPen(QColor(30, 136, 229));
    humidityPen.setWidth(2);
    ui->plot->graph(1)->setPen(humidityPen);
    ui->plot->graph(1)->setName(QStringLiteral("湿度"));

    const double now = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    ui->plot->xAxis->setRange(now, m_plotWindowSeconds, Qt::AlignRight);
}

void MainWindow::setupConnections()
{
    connect(ui->refreshPortButton, &QPushButton::clicked, this, &MainWindow::refreshPorts);
    connect(ui->openButton, &QPushButton::clicked, this, &MainWindow::toggleConnection);
    connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::togglePause);
    connect(ui->clearButton, &QPushButton::clicked, this, &MainWindow::clearData);
    connect(ui->exportButton, &QPushButton::clicked, this, &MainWindow::exportCsv);
    connect(ui->sendTestButton, &QPushButton::clicked, this, &MainWindow::sendTestFrame);
    connect(ui->recordButton, &QPushButton::clicked, this, &MainWindow::toggleRecording);
    connect(ui->clearTableButton, &QPushButton::clicked, this, &MainWindow::clearTable);

    connect(ui->windowCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        m_plotWindowSeconds = ui->windowCombo->currentData().toInt();

        // 暂停或用户已经手动缩放时，保留当前右边界，只改变窗口宽度；
        // 否则跟随最新数据；没有数据时用当前时间作为右边界。
        double end;
        if (m_plotPaused || !m_autoFollow) {
            end = ui->plot->xAxis->range().upper;
        } else if (m_history.isEmpty()) {
            end = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        } else {
            end = m_history.last().timestamp.toMSecsSinceEpoch() / 1000.0;
        }

        ui->plot->xAxis->setRange(end, m_plotWindowSeconds, Qt::AlignRight);
        ui->plot->replot(QCustomPlot::rpQueuedReplot);
        saveSettings();
    });

    // 这些设置一改就落盘，避免程序被强制结束没走 closeEvent 时丢失。
    const auto saveOnChange = [this] { saveSettings(); };
    connect(ui->baudCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, saveOnChange);
    connect(ui->dataBitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, saveOnChange);
    connect(ui->parityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, saveOnChange);
    connect(ui->stopBitsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, saveOnChange);
    connect(ui->highSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, saveOnChange);
    connect(ui->lowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, saveOnChange);
    connect(ui->autoFollowCheckBox, &QCheckBox::toggled, this, saveOnChange);
    connect(ui->portCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, saveOnChange);

    connect(ui->autoFollowCheckBox, &QCheckBox::toggled, this, &MainWindow::setAutoFollow);

    // 用户一拖动或缩放图表，就停止自动跟随，方便查看历史数据。
    connect(ui->plot, &QCustomPlot::mousePress, this, [this](QMouseEvent *) {
        setAutoFollow(false);
    });
    connect(ui->plot, &QCustomPlot::mouseWheel, this, [this](QWheelEvent *) {
        setAutoFollow(false);
    });

    const auto recheckAlarm = [this](double) {
        if (!m_history.isEmpty())
            updateAlarm(m_history.last().temperature);
    };
    connect(ui->highSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, recheckAlarm);
    connect(ui->lowSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, recheckAlarm);

    connect(ui->actionExport, &QAction::triggered, this, &MainWindow::exportCsv);
    connect(ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::showAbout);

    connect(&m_serial, &Transport::dataReceived, this, &MainWindow::onSerialData);
    connect(&m_serial, &Transport::stateChanged, this, &MainWindow::onSerialStateChanged);
    connect(&m_serial, &Transport::errorOccurred, this, &MainWindow::onSerialError);
    connect(&m_virtualSerial, &Transport::dataReceived, this, &MainWindow::onSerialData);
    connect(&m_virtualSerial, &Transport::stateChanged, this, &MainWindow::onSerialStateChanged);
    connect(&m_virtualSerial, &Transport::errorOccurred, this, &MainWindow::onSerialError);
    connect(&m_parser, &FrameParser::sampleReceived, this, &MainWindow::onSampleReceived);
    connect(&m_parser, &FrameParser::errorOccurred, this, &MainWindow::onParserError);
    connect(&m_recorder, &Recorder::stateChanged, this, &MainWindow::onRecorderStateChanged);
    connect(&m_recorder, &Recorder::errorOccurred, this, &MainWindow::onRecorderError);
}

void MainWindow::refreshPorts()
{
    const QString previous = ui->portCombo->currentData().toString();

    // 刷新时先屏蔽信号，避免 clear()/addItem() 触发中间态保存。
    const QSignalBlocker blocker(ui->portCombo);

    ui->portCombo->clear();
    const QStringList ports = SerialManager::availablePorts();
    for (const QString &port : ports) {
        const QString description = SerialManager::portDescription(port);
        const QString text = description.isEmpty()
                                 ? port
                                 : QStringLiteral("%1 (%2)").arg(port, description);
        ui->portCombo->addItem(text, port);
    }

    if (!previous.isEmpty()) {
        const int index = ui->portCombo->findData(previous);
        if (index >= 0)
            ui->portCombo->setCurrentIndex(index);
    }

    // 虚拟串口始终存在，没有硬件时也能完整演示“打开 → 收数据”的流程。
    ui->portCombo->addItem(QStringLiteral("虚拟串口（模拟数据）"), QStringLiteral("__virtual__"));
}

SerialConfig MainWindow::currentSerialConfig() const
{
    SerialConfig config;
    config.portName = ui->portCombo->currentData().toString();
    config.baudRate = ui->baudCombo->currentData().toInt();
    config.dataBits = static_cast<QSerialPort::DataBits>(ui->dataBitsCombo->currentData().toInt());
    config.parity = static_cast<QSerialPort::Parity>(ui->parityCombo->currentData().toInt());
    config.stopBits = static_cast<QSerialPort::StopBits>(ui->stopBitsCombo->currentData().toInt());
    return config;
}

void MainWindow::toggleConnection()
{
    if (isTransportOpen()) {
        closeTransport();
        return;
    }

    QString error;
    if (!openSelectedTransport(&error))
        QMessageBox::critical(this, QStringLiteral("打开失败"), error);
}

bool MainWindow::openSelectedTransport(QString *errorMessage)
{
    const QString portName = ui->portCombo->currentData().toString();
    if (portName == QStringLiteral("__virtual__"))
        m_activeTransport = &m_virtualSerial;
    else
        m_activeTransport = &m_serial;

    if (!m_activeTransport->open(currentSerialConfig(), errorMessage)) {
        m_activeTransport = nullptr;
        return false;
    }

    m_parser.reset();
    m_bytesReceived = 0;
    m_lastSampleTime = QDateTime();
    updateStatusBar();
    updateDataFreshness();
    return true;
}

void MainWindow::closeTransport()
{
    if (!m_activeTransport)
        return;

    m_activeTransport->close();
    m_activeTransport = nullptr;
}

bool MainWindow::isTransportOpen() const
{
    return m_activeTransport && m_activeTransport->isOpen();
}

void MainWindow::onSerialStateChanged(bool opened)
{
    const bool virtualPort = (m_activeTransport == &m_virtualSerial);
    if (!opened)
        m_activeTransport = nullptr;

    updateConnectionUi(opened);

    if (opened) {
        statusBar()->showMessage(virtualPort
                                     ? QStringLiteral("虚拟串口已打开（模拟数据）")
                                     : QStringLiteral("串口已打开"));
    } else {
        statusBar()->showMessage(QStringLiteral("串口已关闭"));
    }

    updateDataFreshness();
}

void MainWindow::updateConnectionUi(bool opened)
{
    ui->portCombo->setEnabled(!opened);
    ui->refreshPortButton->setEnabled(!opened);
    ui->baudCombo->setEnabled(!opened);
    ui->dataBitsCombo->setEnabled(!opened);
    ui->parityCombo->setEnabled(!opened);
    ui->stopBitsCombo->setEnabled(!opened);
    ui->openButton->setText(opened ? QStringLiteral("关闭串口")
                                   : QStringLiteral("打开串口"));
}

void MainWindow::onSerialData(const QByteArray &data)
{
    m_bytesReceived += static_cast<quint64>(data.size());
    m_parser.feed(data);
}

void MainWindow::onSerialError(const QString &message)
{
    qDebug() << "串口错误:" << message;
    statusBar()->showMessage(QStringLiteral("串口错误：%1").arg(message), 5000);
}

void MainWindow::onSampleReceived(const Sample &sample)
{
    m_lastSampleTime = QDateTime::currentDateTime();

    m_history.append(sample);
    if (m_history.size() > 50000)
        m_history.removeFirst();

    m_statistics.add(sample);

    updateCurrentValue(sample);
    updateStatistics();
    appendToTable(sample);
    appendToPlot(sample);
    updateAlarm(sample.temperature);
    m_recorder.record(sample);
    updateDataFreshness();
    updateStatusBar();
}

void MainWindow::onParserError(const QString &message)
{
    qDebug() << "解析错误:" << message;
    statusBar()->showMessage(QStringLiteral("解析错误：%1").arg(message), 3000);
}

void MainWindow::appendToPlot(const Sample &sample)
{
    const double key = sample.timestamp.toMSecsSinceEpoch() / 1000.0;
    ui->plot->graph(0)->addData(key, sample.temperature);
    if (sample.hasHumidity())
        ui->plot->graph(1)->addData(key, sample.humidity);

    // 只保留最近 10 分钟的数据，防止内存一直增长。
    ui->plot->graph(0)->data()->removeBefore(key - 600.0);
    ui->plot->graph(1)->data()->removeBefore(key - 600.0);

    // 暂停时继续收数据，但不重绘；未暂停时，只有“自动跟随”才移动时间窗口。
    if (!m_plotPaused) {
        if (m_autoFollow)
            ui->plot->xAxis->setRange(key, m_plotWindowSeconds, Qt::AlignRight);
        ui->plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void MainWindow::appendToTable(const Sample &sample)
{
    ui->sampleTable->insertRow(0);

    auto *timeItem = new QTableWidgetItem(sample.timestamp.toString(QStringLiteral("HH:mm:ss")));
    auto *temperatureItem = new QTableWidgetItem(QString::number(sample.temperature, 'f', 2));
    auto *humidityItem = new QTableWidgetItem(
        sample.hasHumidity() ? QString::number(sample.humidity, 'f', 1) : QStringLiteral("--"));

    QString statusText = QStringLiteral("正常");
    QColor statusColor(0x2e, 0x7d, 0x32);
    if (sample.temperature > ui->highSpin->value()) {
        statusText = QStringLiteral("高温");
        statusColor = QColor(0xc6, 0x28, 0x28);
    } else if (sample.temperature < ui->lowSpin->value()) {
        statusText = QStringLiteral("低温");
        statusColor = QColor(0x15, 0x65, 0xc0);
    }

    auto *statusItem = new QTableWidgetItem(statusText);
    statusItem->setForeground(statusColor);

    const QList<QTableWidgetItem *> items = {timeItem, temperatureItem, humidityItem, statusItem};
    for (int column = 0; column < items.size(); ++column) {
        items.at(column)->setTextAlignment(Qt::AlignCenter);
        ui->sampleTable->setItem(0, column, items.at(column));
    }

    while (ui->sampleTable->rowCount() > 200)
        ui->sampleTable->removeRow(200);
}

void MainWindow::updateCurrentValue(const Sample &sample)
{
    ui->tempValueLabel->setText(QStringLiteral("%1 ℃").arg(sample.temperature, 0, 'f', 2));
    ui->humidityValueLabel->setText(
        sample.hasHumidity() ? QStringLiteral("%1 %").arg(sample.humidity, 0, 'f', 1)
                             : QStringLiteral("--"));
    ui->timeValueLabel->setText(sample.timestamp.toString(QStringLiteral("hh:mm:ss")));
}

void MainWindow::updateStatistics()
{
    if (m_statistics.isEmpty()) {
        ui->countLabel->setText(QStringLiteral("0"));
        ui->minLabel->setText(QStringLiteral("--"));
        ui->maxLabel->setText(QStringLiteral("--"));
        ui->avgLabel->setText(QStringLiteral("--"));
        return;
    }

    ui->countLabel->setText(QString::number(m_statistics.count()));
    ui->minLabel->setText(QStringLiteral("%1 ℃").arg(m_statistics.minTemperature(), 0, 'f', 2));
    ui->maxLabel->setText(QStringLiteral("%1 ℃").arg(m_statistics.maxTemperature(), 0, 'f', 2));
    ui->avgLabel->setText(QStringLiteral("%1 ℃").arg(m_statistics.averageTemperature(), 0, 'f', 2));
}

void MainWindow::updateAlarm(double temperature)
{
    QString text;
    QString color;

    if (temperature > ui->highSpin->value()) {
        text = QStringLiteral("高温报警");
        color = QStringLiteral("#c62828");
    } else if (temperature < ui->lowSpin->value()) {
        text = QStringLiteral("低温报警");
        color = QStringLiteral("#1565c0");
    } else {
        text = QStringLiteral("正常");
        color = QStringLiteral("#2e7d32");
    }

    ui->alarmLabel->setText(text);
    ui->alarmLabel->setStyleSheet(
        QStringLiteral("color:%1; font-weight:bold;").arg(color));
    ui->tempValueLabel->setStyleSheet(QStringLiteral("color:%1;").arg(color));
}

void MainWindow::togglePause()
{
    m_plotPaused = !m_plotPaused;
    ui->pauseButton->setText(m_plotPaused ? QStringLiteral("继续")
                                          : QStringLiteral("暂停"));

    if (!m_plotPaused) {
        if (m_autoFollow)
            jumpToLatest();
        else
            ui->plot->replot(QCustomPlot::rpQueuedReplot);
    }
}

void MainWindow::setAutoFollow(bool enabled)
{
    // 保持复选框和内部状态一致；用 blocker 避免再次触发 toggled。
    if (ui->autoFollowCheckBox->isChecked() != enabled) {
        QSignalBlocker blocker(ui->autoFollowCheckBox);
        ui->autoFollowCheckBox->setChecked(enabled);
    }

    if (m_autoFollow == enabled)
        return;

    m_autoFollow = enabled;

    // 重新打开自动跟随时，立刻回到最新数据。
    if (enabled)
        jumpToLatest();
}

void MainWindow::jumpToLatest()
{
    const double end = m_history.isEmpty()
                           ? QDateTime::currentMSecsSinceEpoch() / 1000.0
                           : m_history.last().timestamp.toMSecsSinceEpoch() / 1000.0;
    ui->plot->xAxis->setRange(end, m_plotWindowSeconds, Qt::AlignRight);
    ui->plot->replot(QCustomPlot::rpQueuedReplot);
}

void MainWindow::clearData()
{
    m_history.clear();
    m_statistics.reset();

    ui->plot->graph(0)->data()->clear();
    ui->plot->graph(1)->data()->clear();

    if (m_autoFollow)
        jumpToLatest();
    else
        ui->plot->replot(QCustomPlot::rpQueuedReplot);

    clearTable();
    ui->tempValueLabel->setText(QStringLiteral("--"));
    ui->humidityValueLabel->setText(QStringLiteral("--"));
    ui->timeValueLabel->setText(QStringLiteral("--"));
    ui->tempValueLabel->setStyleSheet(QString());
    ui->alarmLabel->setText(QStringLiteral("正常"));
    ui->alarmLabel->setStyleSheet(QStringLiteral("color:#2e7d32; font-weight:bold;"));

    updateStatistics();
    updateStatusBar();
}

void MainWindow::clearTable()
{
    ui->sampleTable->setRowCount(0);
}

void MainWindow::updateDataFreshness()
{
    if (!m_staleLabel)
        return;

    if (!isTransportOpen()) {
        m_staleLabel->setText(QStringLiteral("未连接"));
        m_staleLabel->setStyleSheet(QStringLiteral("color:#757575;"));
        return;
    }

    if (!m_lastSampleTime.isValid()) {
        m_staleLabel->setText(QStringLiteral("等待数据"));
        m_staleLabel->setStyleSheet(QStringLiteral("color:#757575;"));
        return;
    }

    const qint64 ageMs =
        qMax<qint64>(0, m_lastSampleTime.msecsTo(QDateTime::currentDateTime()));
    const double ageSeconds = ageMs / 1000.0;

    if (ageMs > kStaleThresholdMs) {
        m_staleLabel->setText(
            QStringLiteral("数据已过期（最后更新 %1 秒前）").arg(ageSeconds, 0, 'f', 1));
        m_staleLabel->setStyleSheet(QStringLiteral("color:#c62828; font-weight:bold;"));
    } else {
        m_staleLabel->setText(
            QStringLiteral("数据正常（最后更新 %1 秒前）").arg(ageSeconds, 0, 'f', 1));
        m_staleLabel->setStyleSheet(QStringLiteral("color:#2e7d32;"));
    }
}

QString MainWindow::defaultFilePath() const
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QStringLiteral("/SerialTempMonitor");
    QDir().mkpath(directory);
    return QStringLiteral("%1/temperature_%2.csv")
        .arg(directory, QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
}

QString MainWindow::settingsFilePath() const
{
    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (!appDataDir.isEmpty() && QDir().mkpath(appDataDir))
        return appDataDir + QStringLiteral("/settings.ini");

    return QCoreApplication::applicationDirPath() + QStringLiteral("/settings.ini");
}

void MainWindow::exportCsv()
{
    if (m_history.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("导出"),
                                 QStringLiteral("还没有采集到数据。"));
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("导出 CSV"),
                                                defaultFilePath(),
                                                QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::critical(this, QStringLiteral("导出失败"), file.errorString());
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);
    stream << QStringLiteral("时间,温度(℃),湿度(%)\n");

    for (const Sample &sample : m_history) {
        stream << sample.timestamp.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz")) << ','
               << QString::number(sample.temperature, 'f', 2) << ',';
        if (sample.hasHumidity())
            stream << QString::number(sample.humidity, 'f', 2);
        stream << '\n';
    }

    stream.flush();
    file.close();

    QMessageBox::information(
        this, QStringLiteral("导出完成"),
        QStringLiteral("已导出 %1 条记录到：\n%2").arg(m_history.size()).arg(path));
}

void MainWindow::toggleRecording()
{
    if (m_recorder.isRecording()) {
        m_recorder.stop();
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("选择记录文件"),
                                                defaultFilePath(),
                                                QStringLiteral("CSV 文件 (*.csv)"));
    if (path.isEmpty())
        return;
    if (!path.endsWith(QStringLiteral(".csv"), Qt::CaseInsensitive))
        path += QStringLiteral(".csv");

    m_recordPath = path;
    m_recorder.start(path);
}

void MainWindow::sendTestFrame()
{
    if (!isTransportOpen()) {
        QMessageBox::information(this, QStringLiteral("发送测试帧"),
                                 QStringLiteral("请先打开串口（或选择虚拟串口）。"));
        return;
    }

    const double temperature = 20.0 + QRandomGenerator::global()->bounded(1000) / 100.0;
    const double humidity = 40.0 + QRandomGenerator::global()->bounded(2000) / 100.0;
    const QByteArray frame = FrameCodec::encode(temperature, humidity);

    m_activeTransport->write(frame);
    statusBar()->showMessage(
        QStringLiteral("已发送测试帧：%1（若 TX/RX 短接，应能收到）")
            .arg(QString::fromLatin1(frame).trimmed()),
        5000);
}

void MainWindow::onRecorderStateChanged(bool recording)
{
    setRecordingUi(recording, m_recordPath);
}

void MainWindow::onRecorderError(const QString &message)
{
    QMessageBox::warning(this, QStringLiteral("记录失败"), message);
}

void MainWindow::setRecordingUi(bool recording, const QString &filePath)
{
    ui->recordButton->setText(recording ? QStringLiteral("停止记录")
                                        : QStringLiteral("开始记录"));

    if (recording) {
        ui->recordPathLabel->setText(
            QStringLiteral("记录中：%1").arg(QFileInfo(filePath).fileName()));
        ui->recordPathLabel->setToolTip(filePath);
    } else {
        ui->recordPathLabel->setText(QStringLiteral("未记录"));
        ui->recordPathLabel->setToolTip(QString());
    }

    updateStatusBar();
}

void MainWindow::updateStatusBar()
{
    const QString recordState =
        m_recorder.isRecording() ? QStringLiteral(" ｜ 正在记录") : QString();
    statusBar()->showMessage(QStringLiteral("帧数 %1 ｜ 解析错误 %2 ｜ 接收 %3 字节%4")
                                 .arg(m_parser.frameCount())
                                 .arg(m_parser.errorCount())
                                 .arg(m_bytesReceived)
                                 .arg(recordState));
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this, QStringLiteral("关于"),
        QStringLiteral(
            "<h3>串口温度监控上位机</h3>"
            "<p>Qt6 + QSerialPort + QCustomPlot。</p>"
            "<p>帧格式：<code>$D,T=23.50,H=45.20*37</code><br>"
            "其中 37 是载荷的 XOR 校验。</p>"
            "<p>功能：串口参数配置、粘包/半包解析、双轴实时曲线、"
            "报警阈值、统计、CSV 导出、后台记录、模拟数据。</p>"));
}

void MainWindow::loadSettings()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);

    const QString port = settings.value(QStringLiteral("serial/port")).toString();
    if (!port.isEmpty())
        setComboData(ui->portCombo, port);

    setComboData(ui->baudCombo, settings.value(QStringLiteral("serial/baud"), 115200).toInt());
    setComboData(ui->dataBitsCombo,
                 settings.value(QStringLiteral("serial/dataBits"),
                                static_cast<int>(QSerialPort::Data8))
                     .toInt());
    setComboData(ui->parityCombo,
                 settings.value(QStringLiteral("serial/parity"),
                                static_cast<int>(QSerialPort::NoParity))
                     .toInt());
    setComboData(ui->stopBitsCombo,
                 settings.value(QStringLiteral("serial/stopBits"),
                                static_cast<int>(QSerialPort::OneStop))
                     .toInt());

    ui->highSpin->setValue(settings.value(QStringLiteral("alarm/high"), 30.0).toDouble());
    ui->lowSpin->setValue(settings.value(QStringLiteral("alarm/low"), 20.0).toDouble());
    setComboData(ui->windowCombo, settings.value(QStringLiteral("plot/window"), 60).toInt());

    m_plotWindowSeconds = ui->windowCombo->currentData().toInt();

    setAutoFollow(settings.value(QStringLiteral("plot/autoFollow"), true).toBool());
}

void MainWindow::saveSettings()
{
    if (m_loadingSettings)
        return;

    QSettings settings(settingsFilePath(), QSettings::IniFormat);

    settings.setValue(QStringLiteral("serial/port"), ui->portCombo->currentData().toString());
    settings.setValue(QStringLiteral("serial/baud"), ui->baudCombo->currentData().toInt());
    settings.setValue(QStringLiteral("serial/dataBits"), ui->dataBitsCombo->currentData().toInt());
    settings.setValue(QStringLiteral("serial/parity"), ui->parityCombo->currentData().toInt());
    settings.setValue(QStringLiteral("serial/stopBits"), ui->stopBitsCombo->currentData().toInt());
    settings.setValue(QStringLiteral("alarm/high"), ui->highSpin->value());
    settings.setValue(QStringLiteral("alarm/low"), ui->lowSpin->value());
    settings.setValue(QStringLiteral("plot/window"), ui->windowCombo->currentData().toInt());
    settings.setValue(QStringLiteral("plot/autoFollow"), m_autoFollow);

    // 立即刷盘，避免程序随后被强制结束导致这次修改丢失。
    settings.sync();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSettings();

    if (m_recorder.isRecording())
        m_recorder.stop();
    closeTransport();

    QMainWindow::closeEvent(event);
}
