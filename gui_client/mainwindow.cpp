#include "mainwindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QComboBox>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QList>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointF>
#include <QPointer>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSizePolicy>
#include <QStackedLayout>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRectF>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextEdit>
#include <QtMath>
#include <QVBoxLayout>
#include <QVector>
#include <QWidget>

namespace {

class AccentCanvas : public QWidget
{
public:
    explicit AccentCanvas(QWidget *parent = 0) : QWidget(parent) {}

protected:
    void paintEvent(QPaintEvent *event)
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QLinearGradient background(rect().topLeft(), rect().bottomRight());
        background.setColorAt(0.0, QColor("#f8fbff"));
        background.setColorAt(0.55, QColor("#eef4f8"));
        background.setColorAt(1.0, QColor("#eaf7f2"));
        painter.fillRect(rect(), background);

        painter.setPen(QPen(QColor(28, 111, 122, 34), 2));
        painter.drawLine(width() * 0.08, height() * 0.22, width() * 0.42, height() * 0.09);
        painter.drawLine(width() * 0.60, height() * 0.18, width() * 0.91, height() * 0.38);
        painter.drawLine(width() * 0.12, height() * 0.82, width() * 0.47, height() * 0.70);

        painter.setPen(QPen(QColor(28, 111, 122, 70), 2));
        painter.setBrush(QColor(255, 255, 255, 95));
        painter.drawEllipse(QPointF(width() * 0.13, height() * 0.22), 54, 54);
        painter.drawEllipse(QPointF(width() * 0.88, height() * 0.39), 76, 76);
        painter.drawEllipse(QPointF(width() * 0.50, height() * 0.76), 42, 42);
    }
};

}

class PeerMapWidget : public QWidget
{
public:
    explicit PeerMapWidget(QWidget *parent = 0) : QWidget(parent)
    {
        setMinimumHeight(280);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    void setCenterLabel(const QString &label)
    {
        m_centerLabel = label;
        update();
    }

    void setPeers(const QStringList &labels, const QStringList &states)
    {
        m_labels = labels;
        m_states = states;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event)
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#fbfdfe"));

        QPointF center(width() / 2.0, height() / 2.0);
        double radius = qMin(width(), height()) * 0.34;
        if (radius < 82) {
            radius = qMin(width(), height()) * 0.28;
        }

        painter.setPen(QPen(QColor("#c6d8dc"), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(center, radius, radius);

        int peerCount = m_labels.size();
        const double pi = 3.14159265358979323846;
        for (int i = 0; i < peerCount; ++i) {
            double angle = peerCount == 1 ? -pi / 2.0 : (-pi / 2.0) + ((2.0 * pi * i) / peerCount);
            QPointF peer(center.x() + qCos(angle) * radius, center.y() + qSin(angle) * radius);
            QColor lineColor = peerColor(i);
            lineColor.setAlpha(115);
            painter.setPen(QPen(lineColor, 2));
            painter.drawLine(center, peer);
        }

        for (int i = 0; i < peerCount; ++i) {
            double angle = peerCount == 1 ? -pi / 2.0 : (-pi / 2.0) + ((2.0 * pi * i) / peerCount);
            QPointF peer(center.x() + qCos(angle) * radius, center.y() + qSin(angle) * radius);
            drawNode(&painter, peer, 34, peerColor(i), m_labels.at(i), false);
        }

        QString label = m_centerLabel.isEmpty() ? QObject::tr("This wallet") : m_centerLabel;
        drawNode(&painter, center, 50, QColor("#166b76"), label, true);

        if (peerCount == 0) {
            painter.setPen(QColor("#6b7e84"));
            painter.drawText(rect().adjusted(20, 20, -20, -20), Qt::AlignCenter, QObject::tr("No peers reported by the local node."));
        }
    }

private:
    QColor peerColor(int index) const
    {
        QString state = index < m_states.size() ? m_states.at(index) : QString();
        if (state == "reachable") {
            return QColor("#2f8d67");
        }
        if (state == "mismatch") {
            return QColor("#b7791f");
        }
        return QColor("#8aa1a7");
    }

    void drawNode(QPainter *painter, const QPointF &center, int radius, const QColor &color, const QString &label, bool primary)
    {
        painter->setPen(QPen(color.darker(115), primary ? 3 : 2));
        painter->setBrush(primary ? color : QColor("#ffffff"));
        painter->drawEllipse(center, radius, radius);

        painter->setPen(primary ? QColor("#ffffff") : QColor("#153139"));
        QFont font = painter->font();
        font.setBold(true);
        font.setPointSize(primary ? 11 : 9);
        painter->setFont(font);
        QRectF textRect(center.x() - radius + 5, center.y() - radius + 6, radius * 2 - 10, radius * 2 - 12);
        painter->drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, label);
    }

    QString m_centerLabel;
    QStringList m_labels;
    QStringList m_states;
};

class LoadingSpinnerWidget : public QWidget
{
public:
    explicit LoadingSpinnerWidget(QWidget *parent = 0)
        : QWidget(parent),
          m_angle(0)
    {
        setFixedSize(18, 18);
        connect(&m_timer, &QTimer::timeout, this, [this]() {
            m_angle = (m_angle + 30) % 360;
            update();
        });
    }

    void start()
    {
        show();
        if (!m_timer.isActive()) {
            m_timer.start(80);
        }
    }

    void stop()
    {
        m_timer.stop();
        hide();
    }

protected:
    void paintEvent(QPaintEvent *event)
    {
        QWidget::paintEvent(event);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(m_angle);

        const int spokes = 10;
        for (int i = 0; i < spokes; ++i) {
            QColor color("#166b76");
            color.setAlpha(45 + (210 * i / spokes));
            painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
            painter.drawLine(0, -3, 0, -8);
            painter.rotate(360.0 / spokes);
        }
    }

private:
    QTimer m_timer;
    int m_angle;
};

class HistoryChartWidget : public QWidget
{
public:
    explicit HistoryChartWidget(QWidget *parent = 0) : QWidget(parent), m_receivedTotal(0.0), m_outgoingTotal(0.0)
    {
        setMinimumHeight(190);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setSeries(const QVector<QPointF> &received, const QVector<QPointF> &outgoing, double receivedTotal, double outgoingTotal)
    {
        m_received = received;
        m_outgoing = outgoing;
        m_receivedTotal = receivedTotal;
        m_outgoingTotal = outgoingTotal;
        update();
    }

protected:
    void paintEvent(QPaintEvent *event)
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor("#fbfdfe"));

        QRectF plot = rect().adjusted(46, 20, -22, -38);
        if (plot.width() < 40 || plot.height() < 40) {
            return;
        }

        painter.setPen(QPen(QColor("#d7e3e6"), 1));
        painter.drawRect(plot);

        double maxAbs = 1.0;
        updateMaxAbs(m_received, maxAbs);
        updateMaxAbs(m_outgoing, maxAbs);
        double minX = firstX();
        double maxX = lastX();
        if (maxX <= minX) {
            maxX = minX + 1.0;
        }

        double zeroY = valueToY(0.0, plot, maxAbs);
        painter.setPen(QPen(QColor("#afc4c9"), 1));
        painter.drawLine(QPointF(plot.left(), zeroY), QPointF(plot.right(), zeroY));

        drawSeries(&painter, m_received, plot, minX, maxX, maxAbs, QColor("#18735d"));
        drawSeries(&painter, m_outgoing, plot, minX, maxX, maxAbs, QColor("#b3261e"));

        painter.setPen(QColor("#41545a"));
        painter.drawText(QRectF(plot.left(), plot.bottom() + 8, plot.width(), 22), Qt::AlignLeft | Qt::AlignVCenter,
                         QObject::tr("Received +%1 SFR").arg(formatLegendAmount(m_receivedTotal)));
        painter.drawText(QRectF(plot.left(), plot.bottom() + 8, plot.width(), 22), Qt::AlignRight | Qt::AlignVCenter,
                         QObject::tr("Outgoing -%1 SFR").arg(formatLegendAmount(qAbs(m_outgoingTotal))));

        if (m_received.isEmpty() && m_outgoing.isEmpty()) {
            painter.setPen(QColor("#6b7e84"));
            painter.drawText(plot, Qt::AlignCenter, QObject::tr("No wallet history in this range."));
        }
    }

private:
    QString formatLegendAmount(double value) const
    {
        QString text = QString::number(value, 'f', 4);
        while (text.contains(".") && text.endsWith("0")) {
            text.chop(1);
        }
        if (text.endsWith(".")) {
            text.chop(1);
        }
        return text;
    }

    void updateMaxAbs(const QVector<QPointF> &points, double &maxAbs) const
    {
        for (int i = 0; i < points.size(); ++i) {
            maxAbs = qMax(maxAbs, qAbs(points.at(i).y()));
        }
    }

    double firstX() const
    {
        bool hasValue = false;
        double value = 0.0;
        applyFirstX(m_received, hasValue, value);
        applyFirstX(m_outgoing, hasValue, value);
        return value;
    }

    double lastX() const
    {
        bool hasValue = false;
        double value = 0.0;
        applyLastX(m_received, hasValue, value);
        applyLastX(m_outgoing, hasValue, value);
        return value;
    }

    void applyFirstX(const QVector<QPointF> &points, bool &hasValue, double &value) const
    {
        if (points.isEmpty()) {
            return;
        }
        if (!hasValue || points.first().x() < value) {
            value = points.first().x();
            hasValue = true;
        }
    }

    void applyLastX(const QVector<QPointF> &points, bool &hasValue, double &value) const
    {
        if (points.isEmpty()) {
            return;
        }
        if (!hasValue || points.last().x() > value) {
            value = points.last().x();
            hasValue = true;
        }
    }

    double valueToY(double value, const QRectF &plot, double maxAbs) const
    {
        double normalized = value / (maxAbs * 2.2);
        return plot.center().y() - (normalized * plot.height());
    }

    QPointF mapPoint(const QPointF &point, const QRectF &plot, double minX, double maxX, double maxAbs) const
    {
        double x = plot.left() + ((point.x() - minX) / (maxX - minX)) * plot.width();
        return QPointF(x, valueToY(point.y(), plot, maxAbs));
    }

    void drawSeries(QPainter *painter, const QVector<QPointF> &points, const QRectF &plot, double minX, double maxX, double maxAbs, const QColor &color)
    {
        if (points.isEmpty()) {
            return;
        }
        painter->setPen(QPen(color, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        QPointF previous = mapPoint(points.first(), plot, minX, maxX, maxAbs);
        for (int i = 1; i < points.size(); ++i) {
            QPointF current = mapPoint(points.at(i), plot, minX, maxX, maxAbs);
            painter->drawLine(previous, current);
            previous = current;
        }
        painter->setBrush(color);
        painter->setPen(Qt::NoPen);
        for (int i = 0; i < points.size(); ++i) {
            painter->drawEllipse(mapPoint(points.at(i), plot, minX, maxX, maxAbs), 3, 3);
        }
    }

    QVector<QPointF> m_received;
    QVector<QPointF> m_outgoing;
    double m_receivedTotal;
    double m_outgoingTotal;
};

namespace {

QLabel *makeLabel(const QString &text, const QString &objectName)
{
    QLabel *label = new QLabel(text);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    return label;
}

QFrame *makePanel(const QString &objectName)
{
    QFrame *frame = new QFrame;
    frame->setObjectName(objectName);
    frame->setFrameShape(QFrame::NoFrame);
    return frame;
}

QString shortAddress(const QString &address)
{
    if (address.length() <= 18) {
        return address;
    }
    return address.left(10) + QString("...") + address.right(6);
}

QString shortPeerLabel(const QString &url)
{
    QUrl parsed(url);
    QString host = parsed.host();
    if (host.isEmpty()) {
        host = url;
    }
    if (!parsed.isEmpty() && parsed.port() > 0) {
        host += QString(":%1").arg(parsed.port());
    }
    if (host.length() <= 18) {
        return host;
    }
    return host.left(12) + QString("...") + host.right(4);
}

QString namedAccount(const QString &name, const QString &address)
{
    if (address.isEmpty()) {
        return QString("-");
    }
    if (!name.isEmpty()) {
        return QString("%1 (%2)").arg(name).arg(shortAddress(address));
    }
    return shortAddress(address);
}

QString blockchainAccountLabel(const QString &name, const QString &address)
{
    if (address.isEmpty()) {
        return QString("-");
    }
    if (!name.isEmpty()) {
        return QString("%1 (%2)").arg(name).arg(shortAddress(address));
    }
    return shortAddress(address);
}

QString formatSfrAmount(double value)
{
    QString amount = QString::number(value, 'f', 4);
    while (amount.contains(".") && amount.endsWith("0")) {
        amount.chop(1);
    }
    if (amount.endsWith(".")) {
        amount.chop(1);
    }
    if (value > 0) {
        amount.prepend("+");
    }
    return amount + QString(" SFR");
}

QString formatSfrValue(const QString &value)
{
    bool ok = false;
    double parsed = value.toDouble(&ok);
    if (!ok) {
        return value.isEmpty() ? QString("-") : value;
    }
    if (qAbs(parsed) < 0.000001) {
        parsed = 0.0;
    }
    QString amount = QString::number(parsed, 'f', 4);
    while (amount.contains(".") && amount.endsWith("0")) {
        amount.chop(1);
    }
    if (amount.endsWith(".")) {
        amount.chop(1);
    }
    return amount;
}

QString formatSyncDuration(qint64 seconds)
{
    if (seconds < 1) {
        return QObject::tr("less than 1s");
    }
    if (seconds < 60) {
        return QObject::tr("%1s").arg(seconds);
    }
    if (seconds < 3600) {
        return QObject::tr("%1m %2s").arg(seconds / 60).arg(seconds % 60);
    }
    if (seconds < 86400) {
        return QObject::tr("%1h %2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
    }
    return QObject::tr("%1d %2h").arg(seconds / 86400).arg((seconds % 86400) / 3600);
}

bool isPageDataPath(const QString &path)
{
    return path == "/api/network/users" ||
           path == "/api/wallet/history" ||
           path == "/api/mempool" ||
           path == "/api/blockchain/recent" ||
           path == "/api/peers";
}

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_rootStack(new QStackedWidget(this)),
      m_contentStack(0),
      m_terminalProcess(0),
      m_statusTimer(new QTimer(this)),
      m_networkManager(new QNetworkAccessManager(this)),
      m_pendingRequests(),
      m_loadedDataPaths(),
      m_backendPort(4899),
      m_accountRefreshTicks(0),
      m_backendStartBlocked(false),
      m_backendLockDetected(false),
      m_backendRestartPending(false),
      m_loadingAccounts(false),
      m_transactionFee(0.0),
      m_publicKey(QString()),
      m_publicName(QString()),
      m_passwordEdit(0),
      m_loginMessage(0),
      m_loginHintLabel(0),
      m_loginPrimaryButton(0),
      m_loginSkipButton(0),
      m_userLabel(0),
      m_walletTitleLabel(0),
      m_accountCombo(0),
      m_addAccountButton(0),
      m_balanceLabel(0),
      m_estimatedBalanceLabel(0),
      m_pendingBalanceLabel(0),
      m_networkLabel(0),
      m_syncLabel(0),
      m_syncProgressBar(0),
      m_syncEtaLabel(0),
      m_peerLabel(0),
      m_natLabel(0),
      m_membershipJoinedLabel(0),
      m_membershipHeartbeatLabel(0),
      m_membershipCreatorEligibleLabel(0),
      m_currentCreatorLabel(0),
      m_nextCreatorLabel(0),
      m_supplyLabel(0),
      m_ledgerBalanceTotalLabel(0),
      m_supplyDifferenceLabel(0),
      m_userCountLabel(0),
      m_blockCountLabel(0),
      m_latestBlockRecordCountLabel(0),
      m_historyTable(0),
      m_historyLoadingRow(0),
      m_historyLoadingSpinner(0),
      m_historyRangeCombo(0),
      m_historyChart(0),
      m_walletHistoryRecords(),
      m_historyPage(0),
      m_historyPageLabel(0),
      m_historyPrevButton(0),
      m_historyNextButton(0),
      m_mempoolTable(0),
      m_mempoolLoadingRow(0),
      m_mempoolLoadingSpinner(0),
      m_blockchainSearchEdit(0),
      m_blockchainTable(0),
      m_blockchainLoadingRow(0),
      m_blockchainLoadingSpinner(0),
      m_blockchainBlocks(),
      m_blockchainPage(0),
      m_blockchainPageLabel(0),
      m_blockchainPrevButton(0),
      m_blockchainNextButton(0),
      m_peerMap(0),
      m_peersTable(0),
      m_peersLoadingRow(0),
      m_peersLoadingSpinner(0),
      m_contactSearchEdit(0),
      m_networkUsersTable(0),
      m_networkUsersLoadingRow(0),
      m_networkUsersLoadingSpinner(0),
      m_contactsTable(0),
      m_sendContactCombo(0),
      m_sendToEdit(0),
      m_sendAmountEdit(0),
      m_sendFeeEdit(0),
      m_sendMemoEdit(0),
      m_receiveAddressLabel(0),
      m_terminalOutput(0),
      m_terminalInput(0),
      m_terminalStatusLabel(0),
      m_terminalStartButton(0),
      m_terminalStopButton(0),
      m_publicPeerCheckBox(0),
      m_storageProfileComboBox(0),
      m_mainSendButton(0),
      m_joinNetworkButton(0),
      m_balanceButton(0),
      m_sendButton(0),
      m_receiveButton(0),
      m_contactsButton(0),
      m_historyButton(0),
      m_mempoolButton(0),
      m_blockchainButton(0),
      m_peersButton(0),
      m_terminalButton(0),
      m_optionsButton(0),
      m_lastSyncProgress(-1.0),
      m_lastSyncLatestBlock(-1),
      m_lastSyncSampleMs(0)
{
    setWindowTitle(tr("Safire Wallet"));
    resize(1120, 720);
    setMinimumSize(920, 620);

    setStyleSheet(
        "QWidget { color: #152328; font-family: 'Helvetica Neue', Arial; font-size: 13px; }"
        "#LoginCard, #SideBar, #Panel, #AccountCard { background: rgba(255, 255, 255, 235); border: 1px solid #dce7ea; border-radius: 8px; }"
        "#AppTitle { color: #0f2c34; font-size: 30px; font-weight: 700; }"
        "#HeroText { color: #41545a; font-size: 14px; line-height: 145%; }"
        "#SectionTitle { color: #12272d; font-size: 21px; font-weight: 700; }"
        "#SmallTitle { color: #253b42; font-size: 15px; font-weight: 700; }"
        "#Muted { color: #6b7e84; }"
        "#Balance { color: #08282f; font-size: 32px; font-weight: 700; }"
        "#PendingBalance { color: #62767c; font-size: 13px; font-weight: 600; }"
        "#PendingBalance[active='true'] { color: #9a5c00; }"
        "#AccountBalance { color: #0d343b; font-size: 20px; font-weight: 700; }"
        "#StatusGood { color: #18735d; font-weight: 700; }"
        "#Error { color: #b3261e; font-weight: 600; }"
        "#TerminalOutput { background: #0d171b; color: #d9f7ef; border: 1px solid #17343b; border-radius: 6px; font-family: Menlo, Consolas, monospace; font-size: 12px; padding: 8px; }"
        "QLineEdit, QTextEdit, QComboBox { background: #ffffff; border: 1px solid #cddadd; border-radius: 6px; padding: 8px; selection-background-color: #1b7f8a; }"
        "QLineEdit:focus, QTextEdit:focus, QComboBox:focus { border-color: #1b7f8a; }"
        "QPushButton { border-radius: 6px; padding: 8px 12px; font-weight: 700; }"
        "QPushButton#PrimaryButton { background: #166b76; color: white; border: 1px solid #166b76; }"
        "QPushButton#PrimaryButton:hover { background: #0f5963; }"
        "QPushButton#SecondaryButton { background: #edf4f5; color: #17454d; border: 1px solid #c8d9dc; }"
        "QPushButton#SecondaryButton:hover { background: #e1eeee; }"
        "QPushButton#DangerButton { background: #b3261e; color: white; border: 1px solid #b3261e; }"
        "QPushButton#DangerButton:hover { background: #8f1f19; }"
        "QPushButton#NavButton { background: transparent; color: #31525a; border: 1px solid transparent; text-align: left; padding: 9px 10px; }"
        "QPushButton#NavButton:hover { background: #eef6f7; }"
        "QPushButton#NavButton[active='true'] { background: #dceff1; color: #0f5963; border: 1px solid #b8d9dd; }"
        "QProgressBar { background: #edf4f5; border: 1px solid #c8d9dc; border-radius: 6px; height: 12px; text-align: center; color: #31525a; }"
        "QProgressBar::chunk { background: #166b76; border-radius: 5px; }"
        "QTableWidget { background: white; border: 1px solid #dce7ea; border-radius: 6px; gridline-color: #edf2f3; }"
        "QHeaderView::section { background: #edf4f5; color: #41545a; padding: 6px; border: 0; font-weight: 700; }"
        "QFrame#LoadingOverlay { background: rgba(251, 253, 254, 220); border: 1px solid #dce7ea; border-radius: 6px; }"
    );

    m_rootStack->addWidget(createLoginPage());
    m_rootStack->addWidget(createShellPage());
    setCentralWidget(m_rootStack);
    loadContacts();

    m_statusTimer->setInterval(3000);
    connect(m_statusTimer, SIGNAL(timeout()), this, SLOT(refreshWalletStatus()));
    connect(m_networkManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(handleWalletStatusReply(QNetworkReply*)));
}

MainWindow::~MainWindow()
{
    stopTerminal();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    stopTerminal();
    QMainWindow::closeEvent(event);
}

QWidget *MainWindow::createLoginPage()
{
    AccentCanvas *page = new AccentCanvas;
    QHBoxLayout *layout = new QHBoxLayout(page);
    layout->setContentsMargins(72, 58, 72, 58);
    layout->setSpacing(48);

    QVBoxLayout *intro = new QVBoxLayout;
    intro->setSpacing(18);
    intro->addStretch();
    intro->addWidget(makeLabel(tr("Safire"), "AppTitle"));
    intro->addWidget(makeLabel(tr("Proof-of-concept wallet for account balances, transfers, membership, and chain status."), "HeroText"));
    intro->addSpacing(18);
    intro->addWidget(makeLabel(tr("Local wallet lock"), "SmallTitle"));
    intro->addWidget(makeLabel(tr("Protect this GUI with a local password hash, or skip during prototype testing before a password is set."), "Muted"));
    intro->addStretch();

    QFrame *card = makePanel("LoginCard");
    card->setFixedWidth(380);
    QVBoxLayout *form = new QVBoxLayout(card);
    form->setContentsMargins(28, 28, 28, 28);
    form->setSpacing(14);

    form->addWidget(makeLabel(tr("Open Wallet"), "SectionTitle"));
    bool hasPassword = passwordHashExists();
    m_loginHintLabel = makeLabel(hasPassword ? tr("Enter your wallet password to continue.") :
                                               tr("No local password is set yet."), "Muted");
    form->addWidget(m_loginHintLabel);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(hasPassword ? tr("Password") : tr("Choose a password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addWidget(m_passwordEdit);

    m_loginMessage = makeLabel(QString(), "Error");
    m_loginMessage->setMinimumHeight(22);
    form->addWidget(m_loginMessage);

    m_loginPrimaryButton = createPrimaryButton(hasPassword ? tr("Unlock Wallet") : tr("Set Password"));
    form->addWidget(m_loginPrimaryButton);
    m_loginSkipButton = createSecondaryButton(tr("Skip"));
    m_loginSkipButton->setVisible(!hasPassword);
    form->addWidget(m_loginSkipButton);
    form->addWidget(makeLabel(tr("The password file stores only a SHA-256 hash in the program directory."), "Muted"));
    form->addStretch();

    connect(m_loginPrimaryButton, SIGNAL(clicked()), this, SLOT(signIn()));
    connect(m_loginSkipButton, SIGNAL(clicked()), this, SLOT(skipSignIn()));
    connect(m_passwordEdit, SIGNAL(returnPressed()), this, SLOT(signIn()));

    layout->addLayout(intro, 1);
    layout->addWidget(card);
    return page;
}

QWidget *MainWindow::createShellPage()
{
    AccentCanvas *page = new AccentCanvas;
    QHBoxLayout *layout = new QHBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    QFrame *sideBar = makePanel("SideBar");
    sideBar->setFixedWidth(238);
    QVBoxLayout *nav = new QVBoxLayout(sideBar);
    nav->setContentsMargins(16, 16, 16, 16);
    nav->setSpacing(8);

    nav->addWidget(makeLabel(tr("Safire"), "AppTitle"));
    m_userLabel = makeLabel(tr("Wallet locked"), "Muted");
    nav->addWidget(m_userLabel);
    nav->addSpacing(10);

    m_balanceButton = createNavButton(tr("Main"));
    m_sendButton = createNavButton(tr("Send"));
    m_receiveButton = createNavButton(tr("Receive"));
    m_contactsButton = createNavButton(tr("Contacts"));
    m_historyButton = createNavButton(tr("History"));
    m_mempoolButton = createNavButton(tr("Mempool"));
    m_blockchainButton = createNavButton(tr("Blockchain"));
    m_peersButton = createNavButton(tr("Peers"));
    m_terminalButton = createNavButton(tr("Terminal"));
    m_optionsButton = createNavButton(tr("Options"));

    nav->addWidget(m_balanceButton);
    nav->addWidget(m_sendButton);
    nav->addWidget(m_receiveButton);
    nav->addWidget(m_contactsButton);
    nav->addWidget(m_historyButton);
    nav->addWidget(m_mempoolButton);
    nav->addWidget(m_blockchainButton);
    nav->addWidget(m_peersButton);
    nav->addWidget(m_terminalButton);
    nav->addWidget(m_optionsButton);
    nav->addStretch();

    QPushButton *signOutButton = createSecondaryButton(tr("Lock Wallet"));
    nav->addWidget(signOutButton);

    m_contentStack = new QStackedWidget;
    m_contentStack->addWidget(createBalancePage());
    m_contentStack->addWidget(createSendPage());
    m_contentStack->addWidget(createReceivePage());
    m_contentStack->addWidget(createContactsPage());
    m_contentStack->addWidget(createHistoryPage());
    m_contentStack->addWidget(createMempoolPage());
    m_contentStack->addWidget(createBlockchainPage());
    m_contentStack->addWidget(createPeersPage());
    m_contentStack->addWidget(createTerminalPage());
    m_contentStack->addWidget(createOptionsPage());

    connect(m_balanceButton, SIGNAL(clicked()), this, SLOT(showBalance()));
    connect(m_sendButton, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(m_receiveButton, SIGNAL(clicked()), this, SLOT(showReceive()));
    connect(m_contactsButton, SIGNAL(clicked()), this, SLOT(showContacts()));
    connect(m_historyButton, SIGNAL(clicked()), this, SLOT(showHistory()));
    connect(m_mempoolButton, SIGNAL(clicked()), this, SLOT(showMempool()));
    connect(m_blockchainButton, SIGNAL(clicked()), this, SLOT(showBlockchain()));
    connect(m_peersButton, SIGNAL(clicked()), this, SLOT(showPeers()));
    connect(m_terminalButton, SIGNAL(clicked()), this, SLOT(showTerminal()));
    connect(m_optionsButton, SIGNAL(clicked()), this, SLOT(showOptions()));
    connect(signOutButton, SIGNAL(clicked()), this, SLOT(signOut()));

    layout->addWidget(sideBar);
    layout->addWidget(m_contentStack, 1);

    showBalance();
    return page;
}

QWidget *MainWindow::createBalancePage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QFrame *summary = makePanel("Panel");
    QGridLayout *summaryLayout = new QGridLayout(summary);
    summaryLayout->setContentsMargins(22, 18, 22, 18);
    summaryLayout->setSpacing(7);

    m_walletTitleLabel = createSectionTitle(tr("Wallet"));
    summaryLayout->addWidget(m_walletTitleLabel, 0, 0);
    QHBoxLayout *accountRow = new QHBoxLayout;
    accountRow->setContentsMargins(0, 0, 0, 0);
    accountRow->setSpacing(8);
    m_accountCombo = new QComboBox;
    m_accountCombo->setMinimumWidth(260);
    m_addAccountButton = createSecondaryButton(tr("Add Account"));
    accountRow->addWidget(m_accountCombo);
    accountRow->addWidget(m_addAccountButton);
    summaryLayout->addLayout(accountRow, 0, 1, Qt::AlignRight | Qt::AlignVCenter);
    summaryLayout->setColumnStretch(0, 1);
    summaryLayout->setColumnStretch(1, 0);
    summaryLayout->addWidget(makeLabel(tr("Available Balance"), "Muted"), 1, 0, 1, 2);
    m_balanceLabel = makeLabel(tr("-"), "Balance");
    summaryLayout->addWidget(m_balanceLabel, 2, 0, 1, 2);
    m_estimatedBalanceLabel = makeLabel(tr("Estimated: -"), "PendingBalance");
    summaryLayout->addWidget(m_estimatedBalanceLabel, 3, 0, 1, 2);
    m_pendingBalanceLabel = makeLabel(tr("Pending: none"), "PendingBalance");
    summaryLayout->addWidget(m_pendingBalanceLabel, 4, 0, 1, 2);

    QHBoxLayout *actions = new QHBoxLayout;
    m_joinNetworkButton = createPrimaryButton(tr("Join Network"));
    m_mainSendButton = createPrimaryButton(tr("Send"));
    QPushButton *receiveNow = createSecondaryButton(tr("Receive"));
    QPushButton *historyNow = createSecondaryButton(tr("History"));
    QPushButton *setNameNow = createSecondaryButton(tr("Set Name"));
    actions->addWidget(m_joinNetworkButton);
    actions->addWidget(m_mainSendButton);
    actions->addWidget(receiveNow);
    actions->addWidget(historyNow);
    actions->addWidget(setNameNow);
    actions->addStretch();
    summaryLayout->addLayout(actions, 5, 0, 1, 2);

    QFrame *networkPanel = makePanel("Panel");
    QGridLayout *networkLayout = new QGridLayout(networkPanel);
    networkLayout->setContentsMargins(22, 18, 22, 18);
    networkLayout->setSpacing(8);
    networkLayout->setColumnStretch(0, 1);
    networkLayout->setColumnStretch(1, 1);

    networkLayout->addWidget(createSectionTitle(tr("Chain Sync")), 0, 0, 1, 2);
    m_networkLabel = makeLabel(tr("Network: not joined"), "StatusGood");
    networkLayout->addWidget(m_networkLabel, 1, 0, 1, 2);
    m_syncLabel = makeLabel(tr("Sync: waiting for backend"), "Muted");
    networkLayout->addWidget(m_syncLabel, 2, 0, 1, 2);
    m_syncProgressBar = new QProgressBar;
    m_syncProgressBar->setRange(0, 100);
    m_syncProgressBar->setValue(0);
    m_syncProgressBar->setTextVisible(false);
    networkLayout->addWidget(m_syncProgressBar, 3, 0, 1, 2);
    m_syncEtaLabel = makeLabel(tr("Time to sync: -"), "Muted");
    networkLayout->addWidget(m_syncEtaLabel, 4, 0, 1, 2);
    m_peerLabel = makeLabel(tr("Peers: -"), "Muted");
    networkLayout->addWidget(m_peerLabel, 5, 0, 1, 2);
    m_natLabel = makeLabel(tr("Public peer: off"), "Muted");
    networkLayout->addWidget(m_natLabel, 6, 0, 1, 2);

    QFrame *networkInfoPanel = makePanel("Panel");
    QGridLayout *networkInfoLayout = new QGridLayout(networkInfoPanel);
    networkInfoLayout->setContentsMargins(22, 18, 22, 18);
    networkInfoLayout->setSpacing(8);
    networkInfoLayout->setColumnStretch(0, 1);
    networkInfoLayout->setColumnStretch(1, 1);
    networkInfoLayout->setColumnStretch(2, 1);

    networkInfoLayout->addWidget(createSectionTitle(tr("Network Info")), 0, 0, 1, 3);
    m_userCountLabel = makeLabel(tr("Users: -"), "Muted");
    networkInfoLayout->addWidget(m_userCountLabel, 1, 0);
    m_supplyLabel = makeLabel(tr("Issued: -"), "Muted");
    networkInfoLayout->addWidget(m_supplyLabel, 1, 1);
    m_blockCountLabel = makeLabel(tr("Blocks: -"), "Muted");
    networkInfoLayout->addWidget(m_blockCountLabel, 1, 2);
    m_ledgerBalanceTotalLabel = makeLabel(tr("Balances: -"), "Muted");
    networkInfoLayout->addWidget(m_ledgerBalanceTotalLabel, 2, 0, 1, 2);
    m_supplyDifferenceLabel = makeLabel(tr("Difference: -"), "Muted");
    networkInfoLayout->addWidget(m_supplyDifferenceLabel, 2, 2);
    m_latestBlockRecordCountLabel = makeLabel(tr("Latest records: -"), "Muted");
    networkInfoLayout->addWidget(m_latestBlockRecordCountLabel, 3, 0, 1, 3);

    connect(m_joinNetworkButton, SIGNAL(clicked()), this, SLOT(joinNetwork()));
    connect(m_mainSendButton, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(m_addAccountButton, SIGNAL(clicked()), this, SLOT(addWalletAccount()));
    connect(m_accountCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(switchWalletAccount(int)));
    connect(receiveNow, SIGNAL(clicked()), this, SLOT(showReceive()));
    connect(historyNow, SIGNAL(clicked()), this, SLOT(showHistory()));
    connect(setNameNow, SIGNAL(clicked()), this, SLOT(setWalletName()));

    QFrame *membershipPanel = makePanel("AccountCard");
    QGridLayout *membershipLayout = new QGridLayout(membershipPanel);
    membershipLayout->setContentsMargins(16, 14, 16, 14);
    membershipLayout->setSpacing(6);
    membershipLayout->setColumnStretch(0, 1);
    membershipLayout->setColumnStretch(1, 1);
    membershipLayout->addWidget(makeLabel(tr("Membership"), "SmallTitle"), 0, 0, 1, 2);
    m_membershipJoinedLabel = makeLabel(tr("Joined: -"), "Muted");
    membershipLayout->addWidget(m_membershipJoinedLabel, 1, 0);
    m_membershipHeartbeatLabel = makeLabel(tr("Heartbeat: -"), "Muted");
    membershipLayout->addWidget(m_membershipHeartbeatLabel, 1, 1);
    m_membershipCreatorEligibleLabel = makeLabel(tr("Creator eligible: -"), "Muted");
    membershipLayout->addWidget(m_membershipCreatorEligibleLabel, 2, 0, 1, 2);
    m_currentCreatorLabel = makeLabel(tr("Current block: -"), "Muted");
    membershipLayout->addWidget(m_currentCreatorLabel, 3, 0, 1, 2);
    m_nextCreatorLabel = makeLabel(tr("Next block: -"), "Muted");
    membershipLayout->addWidget(m_nextCreatorLabel, 4, 0, 1, 2);

    layout->addWidget(summary);
    layout->addWidget(networkPanel);
    layout->addWidget(membershipPanel);
    layout->addWidget(networkInfoPanel);
    layout->addStretch();
    return page;
}

QWidget *MainWindow::createSendPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("Send Payment")));
    layout->addWidget(makeLabel(tr("Create a draft transfer. Core transaction signing will be wired in a later step."), "Muted"));

    m_sendContactCombo = new QComboBox;
    m_sendContactCombo->addItem(tr("Choose saved contact"), QString());
    layout->addWidget(m_sendContactCombo);

    m_sendToEdit = new QLineEdit;
    m_sendToEdit->setPlaceholderText(tr("Recipient address"));
    layout->addWidget(m_sendToEdit);

    m_sendAmountEdit = new QLineEdit;
    m_sendAmountEdit->setPlaceholderText(tr("Amount in SFR"));
    layout->addWidget(m_sendAmountEdit);

    m_sendFeeEdit = new QLineEdit;
    m_sendFeeEdit->setPlaceholderText(tr("Transfer fee in SFR"));
    layout->addWidget(m_sendFeeEdit);

    m_sendMemoEdit = new QTextEdit;
    m_sendMemoEdit->setPlaceholderText(tr("Memo"));
    m_sendMemoEdit->setFixedHeight(96);
    layout->addWidget(m_sendMemoEdit);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *submit = createPrimaryButton(tr("Review Payment"));
    QPushButton *clear = createSecondaryButton(tr("Clear"));
    actions->addWidget(submit);
    actions->addWidget(clear);
    actions->addStretch();
    layout->addLayout(actions);
    layout->addStretch();

    connect(submit, SIGNAL(clicked()), this, SLOT(submitPayment()));
    connect(clear, SIGNAL(clicked()), m_sendToEdit, SLOT(clear()));
    connect(clear, SIGNAL(clicked()), m_sendAmountEdit, SLOT(clear()));
    connect(clear, SIGNAL(clicked()), m_sendFeeEdit, SLOT(clear()));
    connect(clear, SIGNAL(clicked()), m_sendMemoEdit, SLOT(clear()));
    connect(m_sendContactCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(setSendRecipientFromContact(int)));

    return page;
}

QWidget *MainWindow::createReceivePage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("Receive Payment")));
    layout->addWidget(makeLabel(tr("Share this address to receive Safire payments."), "Muted"));

    QFrame *addressPanel = makePanel("AccountCard");
    QVBoxLayout *addressLayout = new QVBoxLayout(addressPanel);
    addressLayout->setContentsMargins(18, 18, 18, 18);
    addressLayout->setSpacing(10);

    addressLayout->addWidget(makeLabel(tr("Public Address"), "SmallTitle"));
    m_receiveAddressLabel = makeLabel(receiveAddress(), "HeroText");
    m_receiveAddressLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    addressLayout->addWidget(m_receiveAddressLabel);

    QPushButton *copyButton = createPrimaryButton(tr("Copy Address"));
    addressLayout->addWidget(copyButton, 0, Qt::AlignLeft);
    layout->addWidget(addressPanel);
    layout->addStretch();

    connect(copyButton, SIGNAL(clicked()), this, SLOT(copyReceiveAddress()));
    return page;
}

QWidget *MainWindow::createContactsPage()
{
    QWidget *page = new QWidget;
    QHBoxLayout *layout = new QHBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(16);

    QFrame *directoryPanel = makePanel("Panel");
    QVBoxLayout *directoryLayout = new QVBoxLayout(directoryPanel);
    directoryLayout->setContentsMargins(26, 24, 26, 24);
    directoryLayout->setSpacing(14);
    directoryLayout->addWidget(createSectionTitle(tr("Network Users")));

    m_contactSearchEdit = new QLineEdit;
    m_contactSearchEdit->setPlaceholderText(tr("Search by name or address"));
    directoryLayout->addWidget(m_contactSearchEdit);

    m_networkUsersTable = new QTableWidget(0, 3);
    QStringList networkHeaders;
    networkHeaders << tr("Name") << tr("Address") << tr("Balance");
    m_networkUsersTable->setHorizontalHeaderLabels(networkHeaders);
    m_networkUsersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_networkUsersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_networkUsersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_networkUsersTable->verticalHeader()->setVisible(false);
    m_networkUsersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_networkUsersTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_networkUsersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_networkUsersLoadingRow = createLoadingOverlay(&m_networkUsersLoadingSpinner, tr("Loading network users..."));
    directoryLayout->addWidget(wrapWithLoadingOverlay(m_networkUsersTable, m_networkUsersLoadingRow), 1);

    QPushButton *addButton = createPrimaryButton(tr("Add Contact"));
    directoryLayout->addWidget(addButton, 0, Qt::AlignLeft);

    QFrame *contactsPanel = makePanel("Panel");
    QVBoxLayout *contactsLayout = new QVBoxLayout(contactsPanel);
    contactsLayout->setContentsMargins(26, 24, 26, 24);
    contactsLayout->setSpacing(14);
    contactsLayout->addWidget(createSectionTitle(tr("Contacts")));

    m_contactsTable = new QTableWidget(0, 2);
    QStringList contactHeaders;
    contactHeaders << tr("Name") << tr("Address");
    m_contactsTable->setHorizontalHeaderLabels(contactHeaders);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_contactsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_contactsTable->verticalHeader()->setVisible(false);
    m_contactsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_contactsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_contactsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    contactsLayout->addWidget(m_contactsTable, 1);

    QHBoxLayout *contactActions = new QHBoxLayout;
    QPushButton *useButton = createPrimaryButton(tr("Use for Send"));
    QPushButton *removeButton = createSecondaryButton(tr("Remove"));
    contactActions->addWidget(useButton);
    contactActions->addWidget(removeButton);
    contactActions->addStretch();
    contactsLayout->addLayout(contactActions);

    layout->addWidget(directoryPanel, 3);
    layout->addWidget(contactsPanel, 2);

    connect(m_contactSearchEdit, SIGNAL(textChanged(QString)), this, SLOT(filterNetworkUsers(QString)));
    connect(addButton, SIGNAL(clicked()), this, SLOT(addSelectedNetworkUserToContacts()));
    connect(removeButton, SIGNAL(clicked()), this, SLOT(removeSelectedContact()));
    connect(useButton, SIGNAL(clicked()), this, SLOT(useSelectedContactForSend()));

    return page;
}

QWidget *MainWindow::createHistoryPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(createSectionTitle(tr("History")));
    header->addStretch();
    m_historyPrevButton = createSecondaryButton(tr("Previous"));
    m_historyPageLabel = makeLabel(tr("Page 1 of 1"), "Muted");
    m_historyNextButton = createSecondaryButton(tr("Next"));
    header->addWidget(m_historyPrevButton);
    header->addWidget(m_historyPageLabel);
    header->addWidget(m_historyNextButton);
    layout->addLayout(header);

    // Temporarily disabled: chart rendering is too expensive while history grows.
    // m_historyRangeCombo = new QComboBox;
    // m_historyRangeCombo->addItem(tr("7 days"), 7);
    // m_historyRangeCombo->addItem(tr("30 days"), 30);
    // m_historyRangeCombo->addItem(tr("90 days"), 90);
    // m_historyRangeCombo->addItem(tr("All loaded"), 0);
    // m_historyRangeCombo->setCurrentIndex(1);
    // header->addWidget(m_historyRangeCombo);
    // m_historyChart = new HistoryChartWidget;
    // layout->addWidget(m_historyChart);

    m_historyTable = new QTableWidget(0, 5);
    QStringList headers;
    headers << tr("Date") << tr("Type") << tr("Account") << tr("Amount") << tr("Status");
    m_historyTable->setHorizontalHeaderLabels(headers);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setWordWrap(true);
    m_historyLoadingRow = createLoadingOverlay(&m_historyLoadingSpinner, tr("Loading wallet history..."));
    layout->addWidget(wrapWithLoadingOverlay(m_historyTable, m_historyLoadingRow), 1);

    // connect(m_historyRangeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(updateHistoryChart()));
    connect(m_historyPrevButton, SIGNAL(clicked()), this, SLOT(previousHistoryPage()));
    connect(m_historyNextButton, SIGNAL(clicked()), this, SLOT(nextHistoryPage()));
    return page;
}

QWidget *MainWindow::createMempoolPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("Mempool")));
    layout->addWidget(makeLabel(tr("Pending records waiting to be included in a block."), "Muted"));

    m_mempoolTable = new QTableWidget(0, 8);
    QStringList headers;
    headers << tr("#") << tr("Type") << tr("Time") << tr("From / Member") << tr("To") << tr("Amount") << tr("Fee") << tr("Hash");
    m_mempoolTable->setHorizontalHeaderLabels(headers);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_mempoolTable->horizontalHeader()->setSectionResizeMode(7, QHeaderView::ResizeToContents);
    m_mempoolTable->verticalHeader()->setVisible(false);
    m_mempoolTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_mempoolTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mempoolTable->setWordWrap(true);
    m_mempoolLoadingRow = createLoadingOverlay(&m_mempoolLoadingSpinner, tr("Loading mempool..."));
    layout->addWidget(wrapWithLoadingOverlay(m_mempoolTable, m_mempoolLoadingRow), 1);

    return page;
}

QWidget *MainWindow::createBlockchainPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(createSectionTitle(tr("Blockchain")));
    header->addStretch();
    m_blockchainPrevButton = createSecondaryButton(tr("Previous"));
    m_blockchainPageLabel = makeLabel(tr("Page 1 of 1"), "Muted");
    m_blockchainNextButton = createSecondaryButton(tr("Next"));
    header->addWidget(m_blockchainPrevButton);
    header->addWidget(m_blockchainPageLabel);
    header->addWidget(m_blockchainNextButton);
    layout->addLayout(header);
    layout->addWidget(makeLabel(tr("Most recent 50 connected blocks."), "Muted"));

    m_blockchainSearchEdit = new QLineEdit;
    m_blockchainSearchEdit->setPlaceholderText(tr("Search block, creator, hash, record type, name, or address"));
    layout->addWidget(m_blockchainSearchEdit);

    m_blockchainTable = new QTableWidget(0, 7);
    QStringList headers;
    headers << tr("Network") << tr("Block") << tr("Time") << tr("Creator") << tr("Records") << tr("Hash") << tr("Previous");
    m_blockchainTable->setHorizontalHeaderLabels(headers);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_blockchainTable->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_blockchainTable->verticalHeader()->setVisible(false);
    m_blockchainTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_blockchainTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_blockchainTable->setWordWrap(true);
    m_blockchainLoadingRow = createLoadingOverlay(&m_blockchainLoadingSpinner, tr("Loading blockchain records..."));
    layout->addWidget(wrapWithLoadingOverlay(m_blockchainTable, m_blockchainLoadingRow), 1);

    connect(m_blockchainSearchEdit, SIGNAL(textChanged(QString)), this, SLOT(filterBlockchainBlocks(QString)));
    connect(m_blockchainPrevButton, SIGNAL(clicked()), this, SLOT(previousBlockchainPage()));
    connect(m_blockchainNextButton, SIGNAL(clicked()), this, SLOT(nextBlockchainPage()));
    return page;
}

QWidget *MainWindow::createPeersPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("Peers")));
    layout->addWidget(makeLabel(tr("Local peer connections reported by this node."), "Muted"));

    m_peerMap = new PeerMapWidget;
    m_peersLoadingRow = createLoadingOverlay(&m_peersLoadingSpinner, tr("Loading peers..."));
    layout->addWidget(wrapWithLoadingOverlay(m_peerMap, m_peersLoadingRow), 2);

    m_peersTable = new QTableWidget(0, 5);
    QStringList headers;
    headers << tr("Peer") << tr("Reachable") << tr("Genesis") << tr("Latest Block") << tr("Score");
    m_peersTable->setHorizontalHeaderLabels(headers);
    m_peersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_peersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_peersTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_peersTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_peersTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_peersTable->verticalHeader()->setVisible(false);
    m_peersTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_peersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    layout->addWidget(m_peersTable, 1);

    return page;
}

QWidget *MainWindow::createTerminalPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    QHBoxLayout *header = new QHBoxLayout;
    header->addWidget(createSectionTitle(tr("Terminal")));
    header->addStretch();
    m_terminalStatusLabel = makeLabel(tr("Stopped"), "Muted");
    header->addWidget(m_terminalStatusLabel);
    layout->addLayout(header);

    m_terminalOutput = new QPlainTextEdit;
    m_terminalOutput->setObjectName("TerminalOutput");
    m_terminalOutput->setReadOnly(true);
    m_terminalOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_terminalOutput->setMinimumHeight(330);
    m_terminalOutput->appendPlainText(tr("Console stopped."));
    layout->addWidget(m_terminalOutput, 1);

    QHBoxLayout *entry = new QHBoxLayout;
    m_terminalInput = new QLineEdit;
    m_terminalInput->setPlaceholderText(tr("Command"));
    entry->addWidget(m_terminalInput, 1);
    QPushButton *sendButton = createPrimaryButton(tr("Send"));
    entry->addWidget(sendButton);
    layout->addLayout(entry);

    QHBoxLayout *actions = new QHBoxLayout;
    m_terminalStartButton = createSecondaryButton(tr("Start Console"));
    m_terminalStopButton = createSecondaryButton(tr("Stop Console"));
    m_terminalStopButton->setEnabled(false);
    actions->addWidget(m_terminalStartButton);
    actions->addWidget(m_terminalStopButton);
    actions->addStretch();
    layout->addLayout(actions);

    connect(m_terminalStartButton, SIGNAL(clicked()), this, SLOT(startTerminal()));
    connect(m_terminalStopButton, SIGNAL(clicked()), this, SLOT(stopTerminal()));
    connect(sendButton, SIGNAL(clicked()), this, SLOT(sendTerminalCommand()));
    connect(m_terminalInput, SIGNAL(returnPressed()), this, SLOT(sendTerminalCommand()));

    return page;
}

QWidget *MainWindow::createOptionsPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("Options")));
    layout->addWidget(makeLabel(tr("Wallet and node preferences for the proof of concept."), "Muted"));

    QSettings settings("Safire", "SafireWallet");
    QCheckBox *startNetwork = new QCheckBox(tr("Start networking when the wallet opens"));
    QCheckBox *showAdvanced = new QCheckBox(tr("Show advanced chain diagnostics"));
    QCheckBox *confirmBeforeSend = new QCheckBox(tr("Require confirmation before sending"));
    m_publicPeerCheckBox = new QCheckBox(tr("Help the network by accepting peer connections"));
    m_publicPeerCheckBox->setChecked(settings.value("publicPeerMode", false).toBool());
    m_storageProfileComboBox = new QComboBox;
    m_storageProfileComboBox->addItem(tr("Server - full history"), "server");
    m_storageProfileComboBox->addItem(tr("Desktop - 1 year"), "desktop");
    m_storageProfileComboBox->addItem(tr("Mobile - 3 months"), "mobile");
    int storageIndex = m_storageProfileComboBox->findData(storageProfile());
    if (storageIndex < 0) {
        storageIndex = m_storageProfileComboBox->findData("desktop");
    }
    m_storageProfileComboBox->setCurrentIndex(storageIndex);
    confirmBeforeSend->setChecked(true);

    layout->addWidget(startNetwork);
    layout->addWidget(showAdvanced);
    layout->addWidget(confirmBeforeSend);
    layout->addWidget(m_publicPeerCheckBox);
    QHBoxLayout *storageLayout = new QHBoxLayout;
    storageLayout->setContentsMargins(0, 0, 0, 0);
    storageLayout->setSpacing(10);
    storageLayout->addWidget(makeLabel(tr("Storage profile"), "Muted"));
    storageLayout->addWidget(m_storageProfileComboBox, 0, Qt::AlignLeft);
    storageLayout->addStretch();
    layout->addLayout(storageLayout);
    layout->addSpacing(12);
    layout->addWidget(createSectionTitle(tr("Reset Local Blockchain")));
    layout->addWidget(makeLabel(tr("Keeps this wallet but clears local chain, queue, and peer cache data."), "Muted"));
    QPushButton *resetChainButton = createDangerButton(tr("Reset Blockchain Data"));
    layout->addWidget(resetChainButton, 0, Qt::AlignLeft);
    layout->addStretch();
    connect(m_publicPeerCheckBox, SIGNAL(stateChanged(int)), this, SLOT(saveOptions()));
    connect(m_storageProfileComboBox, SIGNAL(currentIndexChanged(int)), this, SLOT(saveOptions()));
    connect(resetChainButton, SIGNAL(clicked()), this, SLOT(resetBlockchain()));
    return page;
}

QWidget *MainWindow::createAccountCard(const QString &name, const QString &balance, const QString &detail)
{
    QFrame *card = makePanel("AccountCard");
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(8);
    layout->addWidget(makeLabel(name, "SmallTitle"));
    layout->addWidget(makeLabel(balance, "AccountBalance"));
    layout->addWidget(makeLabel(detail, "Muted"));
    layout->addStretch();
    return card;
}

QPushButton *MainWindow::createNavButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName("NavButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("active", false);
    return button;
}

QPushButton *MainWindow::createPrimaryButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName("PrimaryButton");
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton *MainWindow::createSecondaryButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName("SecondaryButton");
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QPushButton *MainWindow::createDangerButton(const QString &text)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName("DangerButton");
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

QLabel *MainWindow::createSectionTitle(const QString &text)
{
    return makeLabel(text, "SectionTitle");
}

QWidget *MainWindow::createLoadingOverlay(LoadingSpinnerWidget **spinner, const QString &text)
{
    QFrame *overlay = new QFrame;
    overlay->setObjectName("LoadingOverlay");
    QVBoxLayout *outerLayout = new QVBoxLayout(overlay);
    outerLayout->setContentsMargins(12, 12, 12, 12);
    outerLayout->addStretch();

    QWidget *center = new QWidget(overlay);
    QHBoxLayout *layout = new QHBoxLayout(center);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    LoadingSpinnerWidget *loadingSpinner = new LoadingSpinnerWidget(center);
    QLabel *label = makeLabel(text, "Muted");
    layout->addWidget(loadingSpinner);
    layout->addWidget(label);

    outerLayout->addWidget(center, 0, Qt::AlignCenter);
    outerLayout->addStretch();

    loadingSpinner->stop();
    overlay->hide();
    if (spinner) {
        *spinner = loadingSpinner;
    }
    return overlay;
}

QWidget *MainWindow::wrapWithLoadingOverlay(QWidget *content, QWidget *overlay)
{
    QWidget *container = new QWidget;
    QStackedLayout *layout = new QStackedLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setStackingMode(QStackedLayout::StackAll);
    layout->addWidget(content);
    layout->addWidget(overlay);
    return container;
}

void MainWindow::appendHistory(const QString &date, const QString &type, const QString &account, const QString &amount, const QString &status, const QString &recordKey)
{
    if (!m_historyTable) {
        return;
    }

    int row = m_historyTable->rowCount();
    m_historyTable->insertRow(row);
    QStringList values;
    values << date << type << account << amount << status;
    for (int column = 0; column < values.size(); ++column) {
        QTableWidgetItem *item = new QTableWidgetItem(values.at(column));
        if (!recordKey.isEmpty()) {
            item->setData(Qt::UserRole, recordKey);
        }
        if (column == 3 && amount.startsWith("+")) {
            item->setForeground(QColor("#18735d"));
        } else if (column == 3 && amount.startsWith("-")) {
            item->setForeground(QColor("#b3261e"));
        }
        m_historyTable->setItem(row, column, item);
    }
}

void MainWindow::renderHistoryPage()
{
    if (!m_historyTable) {
        return;
    }

    const int pageSize = 50;
    int totalRecords = m_walletHistoryRecords.size();
    int pageCount = totalRecords > 0 ? ((totalRecords - 1) / pageSize) + 1 : 1;
    if (m_historyPage < 0) {
        m_historyPage = 0;
    }
    if (m_historyPage >= pageCount) {
        m_historyPage = pageCount - 1;
    }

    QString selectedKey = selectedHistoryRecordKey();
    m_historyTable->setUpdatesEnabled(false);
    m_historyTable->setRowCount(0);
    if (totalRecords == 0) {
        appendHistory(tr("-"), tr("History"), tr("Wallet"), tr("-"), tr("No records"));
    } else {
        int start = totalRecords - 1 - (m_historyPage * pageSize);
        int end = start - pageSize + 1;
        if (end < 0) {
            end = 0;
        }

        for (int i = start; i >= end; --i) {
            QJsonObject record = m_walletHistoryRecords.at(i).toObject();

            QString timeValue = record.value("time").toString();
            bool timeOk = false;
            qint64 epoch = timeValue.toLongLong(&timeOk);
            QString date = timeValue;
            if (timeOk && epoch > 0) {
                date = QDateTime::fromSecsSinceEpoch(epoch).toString("yyyy-MM-dd HH:mm");
            }
            if (date.isEmpty()) {
                date = tr("-");
            }

            QString direction = record.value("direction").toString();
            QString fromAccount = namedAccount(record.value("from_name").toString(), record.value("from_key").toString());
            QString toAccount = namedAccount(record.value("to_name").toString(), record.value("to_key").toString());
            QString account = tr("From: %1\nTo: %2").arg(fromAccount).arg(toAccount);
            if (direction == "RECEIVED" || direction == "REWARD") {
                account = tr("From: %1").arg(fromAccount);
            }

            bool netOk = false;
            double netAmount = record.value("net").toString().toDouble(&netOk);
            QString amount = netOk ? formatSfrAmount(netAmount) : record.value("net").toString();

            QString block = record.value("block").toString();
            QString index = record.value("index").toString();
            QString status = tr("Block %1 #%2").arg(block).arg(index);

            appendHistory(date, direction, account, amount, status, historyRecordKey(record));
        }
    }

    m_historyTable->resizeRowsToContents();
    restoreHistorySelection(selectedKey);
    m_historyTable->setUpdatesEnabled(true);

    if (m_historyPageLabel) {
        m_historyPageLabel->setText(tr("Page %1 of %2 (%3 records)")
                                    .arg(m_historyPage + 1)
                                    .arg(pageCount)
                                    .arg(totalRecords));
    }
    if (m_historyPrevButton) {
        m_historyPrevButton->setEnabled(m_historyPage > 0);
    }
    if (m_historyNextButton) {
        m_historyNextButton->setEnabled(m_historyPage + 1 < pageCount);
    }
}

void MainWindow::appendContact(const QString &name, const QString &address)
{
    if (!m_contactsTable || address.trimmed().isEmpty()) {
        return;
    }

    QString trimmedAddress = address.trimmed();
    for (int row = 0; row < m_contactsTable->rowCount(); ++row) {
        QTableWidgetItem *addressItem = m_contactsTable->item(row, 1);
        if (addressItem && addressItem->text() == trimmedAddress) {
            return;
        }
    }

    int row = m_contactsTable->rowCount();
    m_contactsTable->insertRow(row);
    QTableWidgetItem *nameItem = new QTableWidgetItem(name.trimmed().isEmpty() ? shortAddress(trimmedAddress) : name.trimmed());
    QTableWidgetItem *addressItem = new QTableWidgetItem(trimmedAddress);
    nameItem->setData(Qt::UserRole, trimmedAddress);
    addressItem->setData(Qt::UserRole, trimmedAddress);
    m_contactsTable->setItem(row, 0, nameItem);
    m_contactsTable->setItem(row, 1, addressItem);
    m_contactsTable->resizeRowsToContents();
}

void MainWindow::loadContacts()
{
    if (!m_contactsTable) {
        return;
    }

    QSettings settings("Safire", "SafireWallet");
    int count = settings.beginReadArray("contacts");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        appendContact(settings.value("name").toString(), settings.value("address").toString());
    }
    settings.endArray();
    refreshContactDropdown();
}

void MainWindow::saveContacts()
{
    if (!m_contactsTable) {
        return;
    }

    QSettings settings("Safire", "SafireWallet");
    settings.beginWriteArray("contacts", m_contactsTable->rowCount());
    for (int row = 0; row < m_contactsTable->rowCount(); ++row) {
        settings.setArrayIndex(row);
        QTableWidgetItem *nameItem = m_contactsTable->item(row, 0);
        QTableWidgetItem *addressItem = m_contactsTable->item(row, 1);
        settings.setValue("name", nameItem ? nameItem->text() : QString());
        settings.setValue("address", addressItem ? addressItem->text() : QString());
    }
    settings.endArray();
}

void MainWindow::refreshContactDropdown()
{
    if (!m_sendContactCombo) {
        return;
    }

    m_sendContactCombo->blockSignals(true);
    m_sendContactCombo->clear();
    m_sendContactCombo->addItem(tr("Choose saved contact"), QString());
    if (m_contactsTable) {
        for (int row = 0; row < m_contactsTable->rowCount(); ++row) {
            QTableWidgetItem *nameItem = m_contactsTable->item(row, 0);
            QTableWidgetItem *addressItem = m_contactsTable->item(row, 1);
            QString name = nameItem ? nameItem->text() : QString();
            QString address = addressItem ? addressItem->text() : QString();
            if (!address.isEmpty()) {
                m_sendContactCombo->addItem(namedAccount(name, address), address);
            }
        }
    }
    m_sendContactCombo->blockSignals(false);
}

QString MainWindow::historyRecordKey(const QJsonObject &record) const
{
    QStringList parts;
    parts << record.value("block").toString()
          << record.value("index").toString()
          << record.value("direction").toString()
          << record.value("hash").toString()
          << record.value("net").toString();
    return parts.join("|");
}

QString MainWindow::selectedHistoryRecordKey() const
{
    if (!m_historyTable) {
        return QString();
    }

    int row = m_historyTable->currentRow();
    if (row >= 0 && row < m_historyTable->rowCount()) {
        QTableWidgetItem *item = m_historyTable->item(row, 0);
        if (item) {
            QString key = item->data(Qt::UserRole).toString();
            if (!key.isEmpty()) {
                return key;
            }
        }
    }

    QList<QTableWidgetItem *> selectedItems = m_historyTable->selectedItems();
    if (!selectedItems.isEmpty()) {
        QTableWidgetItem *item = selectedItems.first();
        QString key = item->data(Qt::UserRole).toString();
        if (!key.isEmpty()) {
            return key;
        }
    }

    return QString();
}

void MainWindow::restoreHistorySelection(const QString &recordKey)
{
    if (!m_historyTable || recordKey.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_historyTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_historyTable->item(row, 0);
        if (!item) {
            continue;
        }
        if (item->data(Qt::UserRole).toString() == recordKey) {
            m_historyTable->selectRow(row);
            m_historyTable->setCurrentCell(row, 0);
            m_historyTable->scrollToItem(item, QAbstractItemView::EnsureVisible);
            return;
        }
    }
}

QString MainWindow::blockchainBlockKey(const QJsonObject &block) const
{
    QStringList parts;
    parts << block.value("number").toString()
          << block.value("hash").toString();
    return parts.join("|");
}

QString MainWindow::selectedBlockchainBlockKey() const
{
    if (!m_blockchainTable) {
        return QString();
    }

    int row = m_blockchainTable->currentRow();
    if (row >= 0 && row < m_blockchainTable->rowCount()) {
        QTableWidgetItem *item = m_blockchainTable->item(row, 0);
        if (item) {
            QString key = item->data(Qt::UserRole + 1).toString();
            if (!key.isEmpty()) {
                return key;
            }
        }
    }

    QList<QTableWidgetItem *> selectedItems = m_blockchainTable->selectedItems();
    if (!selectedItems.isEmpty()) {
        QTableWidgetItem *item = selectedItems.first();
        QString key = item->data(Qt::UserRole + 1).toString();
        if (!key.isEmpty()) {
            return key;
        }
    }

    return QString();
}

void MainWindow::restoreBlockchainSelection(const QString &blockKey)
{
    if (!m_blockchainTable || blockKey.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_blockchainTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_blockchainTable->item(row, 0);
        if (!item) {
            continue;
        }
        if (item->data(Qt::UserRole + 1).toString() == blockKey && !m_blockchainTable->isRowHidden(row)) {
            m_blockchainTable->selectRow(row);
            m_blockchainTable->setCurrentCell(row, 0);
            m_blockchainTable->scrollToItem(item, QAbstractItemView::EnsureVisible);
            return;
        }
    }
}

QString MainWindow::selectedNetworkUserAddress() const
{
    if (!m_networkUsersTable) {
        return QString();
    }

    int row = m_networkUsersTable->currentRow();
    if (row >= 0 && row < m_networkUsersTable->rowCount()) {
        QTableWidgetItem *item = m_networkUsersTable->item(row, 1);
        if (item) {
            QString address = item->data(Qt::UserRole).toString();
            return address.isEmpty() ? item->text() : address;
        }
    }

    QList<QTableWidgetItem *> selectedItems = m_networkUsersTable->selectedItems();
    if (!selectedItems.isEmpty()) {
        int selectedRow = selectedItems.first()->row();
        QTableWidgetItem *item = m_networkUsersTable->item(selectedRow, 1);
        if (item) {
            QString address = item->data(Qt::UserRole).toString();
            return address.isEmpty() ? item->text() : address;
        }
    }

    return QString();
}

void MainWindow::restoreNetworkUserSelection(const QString &address)
{
    if (!m_networkUsersTable || address.isEmpty()) {
        return;
    }

    for (int row = 0; row < m_networkUsersTable->rowCount(); ++row) {
        QTableWidgetItem *item = m_networkUsersTable->item(row, 1);
        if (!item) {
            continue;
        }
        QString rowAddress = item->data(Qt::UserRole).toString();
        if (rowAddress.isEmpty()) {
            rowAddress = item->text();
        }
        if (rowAddress == address && !m_networkUsersTable->isRowHidden(row)) {
            m_networkUsersTable->selectRow(row);
            m_networkUsersTable->setCurrentCell(row, 0);
            m_networkUsersTable->scrollToItem(item, QAbstractItemView::EnsureVisible);
            return;
        }
    }
}

void MainWindow::applyNetworkUsers(const QString &json)
{
    if (!m_networkUsersTable) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        return;
    }

    QString selectedAddress = selectedNetworkUserAddress();
    QJsonArray users = object.value("users").toArray();
    m_networkUsersTable->setUpdatesEnabled(false);
    m_networkUsersTable->setRowCount(0);
    for (int i = 0; i < users.size(); ++i) {
        QJsonObject user = users.at(i).toObject();
        QString address = user.value("public_key").toString();
        if (address.isEmpty()) {
            continue;
        }
        QString name = user.value("name").toString();
        QString balance = user.value("balance").toString();
        int row = m_networkUsersTable->rowCount();
        m_networkUsersTable->insertRow(row);

        QTableWidgetItem *nameItem = new QTableWidgetItem(name.isEmpty() ? tr("-") : name);
        QTableWidgetItem *addressItem = new QTableWidgetItem(address);
        QTableWidgetItem *balanceItem = new QTableWidgetItem(tr("%1 SFR").arg(balance));
        nameItem->setData(Qt::UserRole, address);
        addressItem->setData(Qt::UserRole, address);
        balanceItem->setData(Qt::UserRole, address);
        m_networkUsersTable->setItem(row, 0, nameItem);
        m_networkUsersTable->setItem(row, 1, addressItem);
        m_networkUsersTable->setItem(row, 2, balanceItem);
    }
    m_networkUsersTable->resizeRowsToContents();
    filterNetworkUsers(m_contactSearchEdit ? m_contactSearchEdit->text() : QString());
    restoreNetworkUserSelection(selectedAddress);
    m_networkUsersTable->setUpdatesEnabled(true);
}

void MainWindow::applySendPaymentResult(const QString &json, bool transportError)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QString status;
    QString message;
    QString hash;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        status = object.value("status").toString();
        message = object.value("message").toString();
        hash = object.value("hash").toString();
    }

    if (message.isEmpty()) {
        message = transportError ? tr("Unable to send transfer request.") : tr("Transfer response was not understood.");
    }

    if (!transportError && status == "ok") {
        if (!hash.isEmpty()) {
            message += tr("\n\nRecord hash: %1").arg(shortAddress(hash));
        }
        QMessageBox::information(this, tr("Payment Sent"), message);
        if (m_sendContactCombo) {
            m_sendContactCombo->setCurrentIndex(0);
        }
        if (m_sendToEdit) {
            m_sendToEdit->clear();
        }
        if (m_sendAmountEdit) {
            m_sendAmountEdit->clear();
        }
        if (m_sendMemoEdit) {
            m_sendMemoEdit->clear();
        }
        refreshWalletStatus();
        showHistory();
        return;
    }

    QMessageBox::warning(this, tr("Payment Not Sent"), message);
}

void MainWindow::applyJoinNetworkResult(const QString &json, bool transportError)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QString status;
    QString message;
    QString hash;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        status = object.value("status").toString();
        message = object.value("message").toString();
        hash = object.value("hash").toString();
    }

    if (message.isEmpty()) {
        message = transportError ? tr("Unable to send join request.") : tr("Join response was not understood.");
    }

    if (!transportError && status == "ok") {
        if (!hash.isEmpty()) {
            message += tr("\n\nRecord hash: %1").arg(shortAddress(hash));
        }
        QMessageBox::information(this, tr("Join Request Sent"), message);
        refreshWalletStatus();
        return;
    }

    QMessageBox::warning(this, tr("Join Not Sent"), message);
}

void MainWindow::applySetNameResult(const QString &json, bool transportError)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QString status;
    QString message;
    QString hash;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        status = object.value("status").toString();
        message = object.value("message").toString();
        hash = object.value("hash").toString();
    }

    if (message.isEmpty()) {
        message = transportError ? tr("Unable to send name update.") : tr("Name update response was not understood.");
    }

    if (!transportError && status == "ok") {
        if (!hash.isEmpty()) {
            message += tr("\n\nRecord hash: %1").arg(shortAddress(hash));
        }
        QMessageBox::information(this, tr("Name Update Sent"), message);
        refreshWalletStatus();
        return;
    }

    QMessageBox::warning(this, tr("Name Not Updated"), message);
}

void MainWindow::applyWalletAccountCreateResult(const QString &json, bool transportError)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QString status;
    QString message;
    QString walletId;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        status = object.value("status").toString();
        message = object.value("message").toString();
        walletId = object.value("wallet_id").toString();
    }

    if (message.isEmpty()) {
        message = transportError ? tr("Unable to create account.") : tr("Create account response was not understood.");
    }

    if (!transportError && status == "ok") {
        QMessageBox::information(this, tr("Account Created"), message);
        if (!walletId.isEmpty()) {
            QUrl url(QString("http://127.0.0.1:%1/api/wallet/accounts/active").arg(m_backendPort));
            QNetworkRequest request(url);
            request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
            QUrlQuery form;
            form.addQueryItem("wallet_id", walletId);
            m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
        }
        refreshWalletStatus();
        return;
    }

    QMessageBox::warning(this, tr("Account Not Created"), message);
}

void MainWindow::applyBlockchainResetResult(const QString &json, bool transportError)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    QString status;
    QString message;
    if (parseError.error == QJsonParseError::NoError && document.isObject()) {
        QJsonObject object = document.object();
        status = object.value("status").toString();
        message = object.value("message").toString();
    }

    if (message.isEmpty()) {
        message = transportError ? tr("Unable to reset blockchain data.") : tr("Reset response was not understood.");
    }

    if (!transportError && status == "ok") {
        m_loadedDataPaths.clear();
        m_walletHistoryRecords = QJsonArray();
        m_blockchainBlocks = QJsonArray();
        m_historyPage = 0;
        m_blockchainPage = 0;
        QMessageBox::information(this, tr("Blockchain Reset"), message + tr("\n\nThe backend will restart and resync."));
        m_backendRestartPending = true;
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: restarting"));
        }
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: restarting backend"));
        }
        stopTerminal();
        if (!m_terminalProcess || m_terminalProcess->state() == QProcess::NotRunning) {
            m_backendRestartPending = false;
            m_backendStartBlocked = false;
            m_backendLockDetected = false;
            QTimer::singleShot(300, this, SLOT(startTerminal()));
            QTimer::singleShot(1000, this, SLOT(refreshWalletStatus()));
        }
        return;
    }

    QMessageBox::warning(this, tr("Reset Failed"), message);
}

void MainWindow::setActiveNav(QPushButton *activeButton)
{
    QList<QPushButton *> buttons;
    buttons << m_balanceButton << m_sendButton << m_receiveButton << m_contactsButton << m_historyButton << m_mempoolButton << m_blockchainButton << m_peersButton << m_terminalButton << m_optionsButton;
    for (int i = 0; i < buttons.size(); ++i) {
        QPushButton *button = buttons.at(i);
        if (!button) {
            continue;
        }
        button->setProperty("active", button == activeButton);
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
}

void MainWindow::setLoadingWidget(QWidget *row, LoadingSpinnerWidget *spinner, bool loading)
{
    if (!row) {
        return;
    }
    row->setVisible(loading);
    if (loading) {
        row->raise();
    }
    if (!spinner) {
        return;
    }
    if (loading) {
        spinner->start();
    } else {
        spinner->stop();
    }
}

void MainWindow::setLoadingState(const QString &path, bool loading)
{
    if (path == "/api/network/users") {
        setLoadingWidget(m_networkUsersLoadingRow, m_networkUsersLoadingSpinner, loading);
    } else if (path == "/api/wallet/history") {
        setLoadingWidget(m_historyLoadingRow, m_historyLoadingSpinner, loading);
    } else if (path == "/api/mempool") {
        setLoadingWidget(m_mempoolLoadingRow, m_mempoolLoadingSpinner, loading);
    } else if (path == "/api/blockchain/recent") {
        setLoadingWidget(m_blockchainLoadingRow, m_blockchainLoadingSpinner, loading);
    } else if (path == "/api/peers") {
        setLoadingWidget(m_peersLoadingRow, m_peersLoadingSpinner, loading);
    }
}

QString MainWindow::receiveAddress() const
{
    return m_publicKey;
}

QString MainWindow::syncEtaText(double syncProgress,
                                bool progressOk,
                                qint64 latestBlockNumber,
                                bool latestBlockOk,
                                qint64 peerLatestBlockNumber,
                                bool peerLatestBlockOk)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    QString text = tr("Time to sync: calculating");

    if (!progressOk) {
        m_lastSyncProgress = -1.0;
        m_lastSyncLatestBlock = -1;
        m_lastSyncSampleMs = 0;
        return tr("Time to sync: -");
    }

    if (syncProgress >= 99.95 || (latestBlockOk && peerLatestBlockOk && latestBlockNumber >= peerLatestBlockNumber)) {
        m_lastSyncProgress = syncProgress;
        m_lastSyncLatestBlock = latestBlockOk ? latestBlockNumber : -1;
        m_lastSyncSampleMs = now;
        return tr("Time to sync: complete");
    }

    if ((m_lastSyncProgress >= 0.0 && syncProgress + 0.5 < m_lastSyncProgress) ||
        (m_lastSyncLatestBlock >= 0 && latestBlockOk && latestBlockNumber < m_lastSyncLatestBlock)) {
        m_lastSyncProgress = syncProgress;
        m_lastSyncLatestBlock = latestBlockOk ? latestBlockNumber : -1;
        m_lastSyncSampleMs = now;
        return text;
    }

    if (m_lastSyncSampleMs > 0) {
        double elapsedSeconds = static_cast<double>(now - m_lastSyncSampleMs) / 1000.0;
        if (elapsedSeconds >= 1.0) {
            if (latestBlockOk && peerLatestBlockOk && peerLatestBlockNumber > latestBlockNumber &&
                m_lastSyncLatestBlock >= 0 && latestBlockNumber > m_lastSyncLatestBlock) {
                double blocksPerSecond = static_cast<double>(latestBlockNumber - m_lastSyncLatestBlock) / elapsedSeconds;
                if (blocksPerSecond > 0.001) {
                    qint64 remainingBlocks = peerLatestBlockNumber - latestBlockNumber;
                    qint64 secondsRemaining = static_cast<qint64>(qCeil(static_cast<double>(remainingBlocks) / blocksPerSecond));
                    text = tr("Time to sync: about %1").arg(formatSyncDuration(secondsRemaining));
                }
            } else if (m_lastSyncProgress >= 0.0 && syncProgress > m_lastSyncProgress && syncProgress < 100.0) {
                double percentPerSecond = (syncProgress - m_lastSyncProgress) / elapsedSeconds;
                if (percentPerSecond > 0.001) {
                    qint64 secondsRemaining = static_cast<qint64>(qCeil((100.0 - syncProgress) / percentPerSecond));
                    text = tr("Time to sync: about %1").arg(formatSyncDuration(secondsRemaining));
                }
            }
        }
    }

    m_lastSyncProgress = syncProgress;
    m_lastSyncLatestBlock = latestBlockOk ? latestBlockNumber : -1;
    m_lastSyncSampleMs = now;
    return text;
}

QString MainWindow::coreBinaryPath() const
{
    QStringList candidates;
    candidates << QDir::current().absoluteFilePath("bin/Safire");

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.absoluteFilePath("../../../../bin/Safire");
    candidates << bundledCoreBinaryPath();

    for (int i = 0; i < candidates.size(); ++i) {
        QFileInfo file(candidates.at(i));
        if (file.exists() && file.isExecutable() && file.isFile()) {
            return file.canonicalFilePath();
        }
    }
    return QString();
}

QString MainWindow::bundledCoreBinaryPath() const
{
    QDir appDir(QCoreApplication::applicationDirPath());
    return appDir.absoluteFilePath("../Resources/bin/Safire");
}

QString MainWindow::bundledConfigFilePath() const
{
    QDir appDir(QCoreApplication::applicationDirPath());
    return appDir.absoluteFilePath("../Resources/safire.conf");
}

QString MainWindow::runtimeDataDirectory() const
{
    QFileInfo bundledCore(bundledCoreBinaryPath());
    if (bundledCore.exists() && bundledCore.isExecutable() && bundledCore.isFile()) {
        QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        if (path.isEmpty()) {
            path = QDir::home().absoluteFilePath(".safire");
        }
        QDir dir(path);
        dir.mkpath(".");

        QString runtimeConfig = dir.absoluteFilePath("safire.conf");
        if (!QFileInfo(runtimeConfig).exists() && QFileInfo(bundledConfigFilePath()).exists()) {
            QFile::copy(bundledConfigFilePath(), runtimeConfig);
        }
        return dir.absolutePath();
    }

    QString corePath = coreBinaryPath();
    if (!corePath.isEmpty()) {
        QFileInfo coreInfo(corePath);
        return QDir(coreInfo.absoluteDir().absolutePath()).absoluteFilePath("..");
    }
    return QDir::currentPath();
}

QString MainWindow::configFilePath() const
{
    return QDir(runtimeDataDirectory()).absoluteFilePath("safire.conf");
}

QString MainWindow::passwordHashFilePath() const
{
    return QDir(runtimeDataDirectory()).absoluteFilePath("safire-gui-password.sha256");
}

QString MainWindow::passwordHashFor(const QString &password) const
{
    QByteArray digest = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256);
    return QString::fromLatin1(digest.toHex());
}

bool MainWindow::passwordHashExists() const
{
    QFileInfo file(passwordHashFilePath());
    return file.exists() && file.isFile() && file.size() > 0;
}

QString MainWindow::readPasswordHash() const
{
    QFile file(passwordHashFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromLatin1(file.readAll()).trimmed();
}

bool MainWindow::savePasswordHash(const QString &hash) const
{
    QFile file(passwordHashFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(hash.toLatin1());
    file.write("\n");
    file.close();
    QFile::setPermissions(passwordHashFilePath(),
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    return true;
}

bool MainWindow::publicPeerModeEnabled() const
{
    QSettings settings("Safire", "SafireWallet");
    return settings.value("publicPeerMode", false).toBool();
}

QString MainWindow::storageProfile() const
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString("desktop");
    }

    while (!file.atEnd()) {
        QString line = QString::fromUtf8(file.readLine()).trimmed();
        int separator = line.indexOf('=');
        if (separator < 0) {
            separator = line.indexOf(':');
        }
        if (separator < 0) {
            continue;
        }
        QString key = line.left(separator).trimmed();
        QString value = line.mid(separator + 1).trimmed().toLower();
        if (key == "storage_profile" || key == "storage") {
            if (value == "server" || value == "full") {
                return QString("server");
            }
            if (value == "mobile" || value == "phone") {
                return QString("mobile");
            }
            return QString("desktop");
        }
    }
    return QString("desktop");
}

bool MainWindow::saveStorageProfile(const QString &profile) const
{
    QString normalized = profile.trimmed().toLower();
    if (normalized != "server" && normalized != "mobile") {
        normalized = "desktop";
    }

    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QStringList lines = QString::fromUtf8(file.readAll()).split('\n');
    file.close();

    bool replaced = false;
    for (int i = 0; i < lines.size(); ++i) {
        QString trimmed = lines.at(i).trimmed();
        int separator = trimmed.indexOf('=');
        if (separator < 0) {
            separator = trimmed.indexOf(':');
        }
        if (separator < 0) {
            continue;
        }
        QString key = trimmed.left(separator).trimmed();
        if (key == "storage_profile" || key == "storage") {
            lines[i] = QString("storage_profile=%1").arg(normalized);
            replaced = true;
        }
    }
    if (!replaced) {
        lines << QString("storage_profile=%1").arg(normalized);
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(lines.join('\n').toUtf8());
    if (!lines.isEmpty() && !lines.last().isEmpty()) {
        file.write("\n");
    }
    file.close();
    return true;
}

bool MainWindow::ensureBackendRunning()
{
    if (m_terminalProcess && m_terminalProcess->state() != QProcess::NotRunning) {
        return true;
    }
    if (m_backendStartBlocked) {
        return false;
    }

    QString corePath = coreBinaryPath();
    if (corePath.isEmpty()) {
        if (m_balanceLabel) {
            m_balanceLabel->setText(tr("-"));
        }
        if (m_estimatedBalanceLabel) {
            m_estimatedBalanceLabel->setText(tr("Estimated: -"));
        }
        if (m_pendingBalanceLabel) {
            m_pendingBalanceLabel->setText(tr("Pending: none"));
        }
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: core binary not found"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 100);
            m_syncProgressBar->setValue(0);
        }
        appendTerminalText(tr("\nUnable to find bin/Safire. Build the core app first.\n"));
        return false;
    }

    if (!m_terminalProcess) {
        m_terminalProcess = new QProcess(this);
        m_terminalProcess->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_terminalProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readTerminalOutput()));
        connect(m_terminalProcess, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(terminalFinished(int,QProcess::ExitStatus)));
        connect(m_terminalProcess, SIGNAL(errorOccurred(QProcess::ProcessError)), this, SLOT(terminalError(QProcess::ProcessError)));
    }

    m_terminalProcess->setWorkingDirectory(runtimeDataDirectory());
    QStringList args;
    args << "--api-port" << QString::number(m_backendPort);
    if (publicPeerModeEnabled()) {
        args << "--enable-nat";
    }
    appendTerminalText(tr("\nStarting console backend: %1\n").arg(corePath));
    m_terminalProcess->start(corePath, args);

    if (!m_terminalProcess->waitForStarted(2000)) {
        appendTerminalText(tr("Unable to start console backend: %1\n").arg(m_terminalProcess->errorString()));
        m_backendStartBlocked = true;
        if (m_terminalStatusLabel) {
            m_terminalStatusLabel->setText(tr("Start failed"));
        }
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: start failed"));
        }
        return false;
    }

    if (m_terminalStatusLabel) {
        m_terminalStatusLabel->setText(tr("Running"));
    }
    if (m_terminalStartButton) {
        m_terminalStartButton->setEnabled(false);
    }
    if (m_terminalStopButton) {
        m_terminalStopButton->setEnabled(true);
    }
    return true;
}

void MainWindow::saveOptions()
{
    QSettings settings("Safire", "SafireWallet");
    if (m_publicPeerCheckBox) {
        settings.setValue("publicPeerMode", m_publicPeerCheckBox->isChecked());
    }
    if (m_storageProfileComboBox) {
        QString profile = m_storageProfileComboBox->itemData(m_storageProfileComboBox->currentIndex()).toString();
        if (!saveStorageProfile(profile)) {
            appendTerminalText(tr("\nUnable to save storage profile to safire.conf.\n"));
        }
    }
    if (m_terminalProcess && m_terminalProcess->state() != QProcess::NotRunning) {
        appendTerminalText(tr("\nOptions saved. Restart the console backend to apply public peer mode changes immediately.\n"));
    }
}

void MainWindow::resetBlockchain()
{
    QString message = tr("Reset local blockchain data for this wallet?\n\nThis keeps wallet.dat, but clears the local chain database, queued records, and peer cache. The node will need to resync from safire.conf/default peers.");
    int answer = QMessageBox::warning(this,
                                      tr("Reset Blockchain"),
                                      message,
                                      QMessageBox::Yes | QMessageBox::Cancel,
                                      QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/blockchain/reset").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("confirm", "reset-local-chain");
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::appendTerminalText(const QString &text)
{
    if (!m_terminalOutput || text.isEmpty()) {
        return;
    }

    QString normalized = text;
    normalized.replace("\r\n", "\n");
    normalized.replace("\r", "\n");
    m_terminalOutput->moveCursor(QTextCursor::End);
    m_terminalOutput->insertPlainText(normalized);
    m_terminalOutput->moveCursor(QTextCursor::End);
}

void MainWindow::applyWalletStatus(const QString &json)
{
    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: invalid status response"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 0);
        }
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: %1").arg(object.value("status").toString()));
        }
        return;
    }

    QString balance = object.value("balance").toString();
    QString pendingDelta = object.value("pending_balance_delta").toString();
    QString estimatedBalance = object.value("estimated_balance").toString();
    QString pendingWalletRecords = object.value("pending_wallet_records").toString();
    QString publicKey = object.value("public_key").toString();
    QString publicName = object.value("public_name").toString();
    QString joined = object.value("joined").toString();
    QString sync = object.value("network_up_to_date").toString();
    QString peerSync = object.value("peer_sync").toString();
    QString peerChainMatch = object.value("peer_chain_match").toString();
    QString latestBlock = object.value("latest_block_id").toString();
    QString latestBlockTime = object.value("latest_block_time").toString();
    QString latestBlockRecordCount = object.value("latest_block_record_count").toString();
    QString peerLatestBlock = object.value("peer_latest_block_id").toString();
    QString peerCount = object.value("local_peers").toString();
    QString natEnabled = object.value("nat_enabled").toString();
    QString natMapped = object.value("nat_mapped").toString();
    QString natMethod = object.value("nat_method").toString();
    QString natExternalAddress = object.value("nat_external_address").toString();
    QString natExternalPort = object.value("nat_external_port").toString();
    QString natMessage = object.value("nat_message").toString();
    QString supply = object.value("currency_supply").toString();
    QString ledgerBalanceTotal = object.value("ledger_balance_total").toString();
    QString supplyDifference = object.value("supply_difference").toString();
    QString userCount = object.value("user_count").toString();
    QString blockCount = object.value("block_count").toString();
    QString heartbeat = object.value("active_heartbeat").toString();
    QString creatorEligible = object.value("creator_eligible").toString();
    QString creatorEligibilityCheckpoint = object.value("creator_eligibility_checkpoint_block").toString();
    QString currentCreator = object.value("current_block_creator").toString();
    QString currentCreatorName = object.value("current_block_creator_name").toString();
    QString currentCreatorIsWallet = object.value("current_block_creator_is_wallet").toString();
    QString nextCreator = object.value("next_block_creator").toString();
    QString nextCreatorName = object.value("next_block_creator_name").toString();
    QString nextCreatorIsWallet = object.value("next_block_creator_is_wallet").toString();
    QString secondsUntilNext = object.value("seconds_until_next_block").toString();
    if (!publicKey.isEmpty()) {
        m_publicKey = publicKey;
    }
    m_publicName = publicName;
    bool feeOk = false;
    double parsedFee = object.value("transaction_fee").toString().toDouble(&feeOk);
    if (feeOk) {
        m_transactionFee = parsedFee;
        if (m_sendFeeEdit && m_sendFeeEdit->text().trimmed().isEmpty()) {
            m_sendFeeEdit->setText(QString::number(m_transactionFee, 'f', 4));
        }
    }
    bool progressOk = false;
    double syncProgress = object.value("sync_progress").toString().toDouble(&progressOk);
    int progressValue = progressOk ? static_cast<int>(syncProgress + 0.5) : 0;
    if (progressValue < 0) {
        progressValue = 0;
    }
    if (progressValue > 100) {
        progressValue = 100;
    }
    bool latestBlockNumberOk = false;
    qint64 latestBlockNumber = latestBlock.toLongLong(&latestBlockNumberOk);
    bool peerLatestBlockNumberOk = false;
    qint64 peerLatestBlockNumber = peerLatestBlock.toLongLong(&peerLatestBlockNumberOk);

    if (m_balanceLabel) {
        m_balanceLabel->setText(tr("%1 SFR").arg(formatSfrValue(balance)));
    }
    bool pendingOk = false;
    double pendingValue = pendingDelta.toDouble(&pendingOk);
    int pendingCount = pendingWalletRecords.toInt();
    bool hasPendingBalanceDelta = pendingOk && qAbs(pendingValue) >= 0.000001;
    if (m_estimatedBalanceLabel) {
        m_estimatedBalanceLabel->setText(tr("Estimated: %1 SFR").arg(formatSfrValue(estimatedBalance)));
        m_estimatedBalanceLabel->setProperty("active", hasPendingBalanceDelta ? "true" : "false");
        m_estimatedBalanceLabel->style()->unpolish(m_estimatedBalanceLabel);
        m_estimatedBalanceLabel->style()->polish(m_estimatedBalanceLabel);
    }
    if (m_pendingBalanceLabel) {
        if (hasPendingBalanceDelta) {
            m_pendingBalanceLabel->setText(tr("Pending settlement: %1 (%2 record%3)")
                .arg(formatSfrAmount(pendingValue))
                .arg(pendingCount)
                .arg(pendingCount == 1 ? QString() : tr("s")));
        } else if (pendingCount > 0) {
            m_pendingBalanceLabel->setText(tr("Pending settlement: %1 non-balance record%2")
                .arg(pendingCount)
                .arg(pendingCount == 1 ? QString() : tr("s")));
        } else {
            m_pendingBalanceLabel->setText(tr("Pending: none"));
        }
        m_pendingBalanceLabel->setProperty("active", (pendingCount > 0) ? "true" : "false");
        m_pendingBalanceLabel->style()->unpolish(m_pendingBalanceLabel);
        m_pendingBalanceLabel->style()->polish(m_pendingBalanceLabel);
    }
    if (m_joinNetworkButton) {
        m_joinNetworkButton->setVisible(joined != "yes");
        m_joinNetworkButton->setEnabled(true);
    }
    if (m_mainSendButton) {
        m_mainSendButton->setVisible(joined == "yes");
    }
    if (m_walletTitleLabel) {
        QString walletLabel = m_publicName.isEmpty() ? shortAddress(m_publicKey) : m_publicName;
        m_walletTitleLabel->setText(walletLabel.isEmpty() ? tr("Wallet") : tr("Wallet: %1").arg(walletLabel));
        if (m_peerMap) {
            m_peerMap->setCenterLabel(walletLabel.isEmpty() ? tr("This wallet") : walletLabel);
        }
    }
    if (m_networkLabel) {
        QString peerSyncDisplay = peerSync.isEmpty() ? tr("-") : peerSync;
        if (peerChainMatch == "no") {
            peerSyncDisplay = tr("fork");
        }
        m_networkLabel->setText(tr("Network up to date: %1  Peer sync: %2").arg(sync).arg(peerSyncDisplay));
    }
    if (m_membershipJoinedLabel) {
        m_membershipJoinedLabel->setText(tr("Joined: %1").arg(joined));
    }
    if (m_membershipHeartbeatLabel) {
        m_membershipHeartbeatLabel->setText(tr("Heartbeat: %1").arg(heartbeat));
    }
    if (m_membershipCreatorEligibleLabel) {
        QString eligibleText = creatorEligible.isEmpty() ? tr("-") : creatorEligible;
        if (!creatorEligibilityCheckpoint.isEmpty() && creatorEligibilityCheckpoint != "0") {
            m_membershipCreatorEligibleLabel->setText(tr("Creator eligible: %1 (checkpoint %2)").arg(eligibleText).arg(creatorEligibilityCheckpoint));
        } else {
            m_membershipCreatorEligibleLabel->setText(tr("Creator eligible: %1").arg(eligibleText));
        }
    }
    if (m_currentCreatorLabel) {
        if (currentCreator.isEmpty()) {
            m_currentCreatorLabel->setText(tr("Current block: no active creator"));
        } else if (currentCreatorIsWallet == "yes") {
            m_currentCreatorLabel->setText(tr("Current block: this wallet"));
        } else {
            m_currentCreatorLabel->setText(tr("Current block: %1").arg(namedAccount(currentCreatorName, currentCreator)));
        }
    }
    if (m_nextCreatorLabel) {
        if (nextCreator.isEmpty()) {
            m_nextCreatorLabel->setText(tr("Next block: no active creator"));
        } else if (nextCreatorIsWallet == "yes") {
            m_nextCreatorLabel->setText(tr("Next block: this wallet in %1s").arg(secondsUntilNext));
        } else {
            m_nextCreatorLabel->setText(tr("Next block: %1 in %2s").arg(namedAccount(nextCreatorName, nextCreator)).arg(secondsUntilNext));
        }
    }
    if (m_syncLabel) {
        QString latestBlockDate = tr("-");
        bool latestTimeOk = false;
        qint64 latestEpoch = latestBlockTime.toLongLong(&latestTimeOk);
        if (latestTimeOk && latestEpoch > 0) {
            latestBlockDate = QDateTime::fromSecsSinceEpoch(latestEpoch).toString("yyyy-MM-dd HH:mm");
        } else if (!latestBlockTime.isEmpty()) {
            latestBlockDate = latestBlockTime;
        } else {
            if (latestBlockNumberOk && latestBlockNumber > 0) {
                latestBlockDate = QDateTime::fromSecsSinceEpoch(latestBlockNumber * 15).toString("yyyy-MM-dd HH:mm");
            }
        }
        if (progressOk) {
            m_syncLabel->setText(tr("Sync: %1%  Latest block: %2  Date: %3").arg(progressValue).arg(latestBlock).arg(latestBlockDate));
        } else {
            m_syncLabel->setText(tr("Sync: %1  Latest block: %2  Date: %3").arg(sync).arg(latestBlock).arg(latestBlockDate));
        }
    }
    if (m_syncProgressBar) {
        if (progressOk) {
            m_syncProgressBar->setRange(0, 100);
            m_syncProgressBar->setValue(progressValue);
        } else {
            m_syncProgressBar->setRange(0, 0);
        }
    }
    if (m_syncEtaLabel) {
        m_syncEtaLabel->setText(syncEtaText(syncProgress,
                                            progressOk,
                                            latestBlockNumber,
                                            latestBlockNumberOk,
                                            peerLatestBlockNumber,
                                            peerLatestBlockNumberOk));
    }
    if (m_peerLabel) {
        QString peerLatestDisplay = (peerLatestBlock.isEmpty() || peerLatestBlock == "-1") ? tr("-") : peerLatestBlock;
        QString peerLatestDate = tr("-");
        if (peerLatestBlockNumberOk && peerLatestBlockNumber > 0) {
            peerLatestDate = QDateTime::fromSecsSinceEpoch(peerLatestBlockNumber * 15).toString("yyyy-MM-dd HH:mm");
        }
        m_peerLabel->setText(tr("Peers: %1  Peer latest block: %2  Date: %3").arg(peerCount).arg(peerLatestDisplay).arg(peerLatestDate));
    }
    if (m_natLabel) {
        if (natEnabled != "yes") {
            m_natLabel->setText(tr("Public peer: off"));
        } else if (natMapped == "yes") {
            QString endpoint = natExternalAddress.isEmpty() ? QString::number(m_backendPort) : tr("%1:%2").arg(natExternalAddress).arg(natExternalPort);
            m_natLabel->setText(tr("Public peer: mapped via %1 (%2)").arg(natMethod.isEmpty() ? tr("router") : natMethod).arg(endpoint));
        } else {
            m_natLabel->setText(tr("Public peer: waiting - %1").arg(natMessage.isEmpty() ? tr("router did not map the port") : natMessage));
        }
    }
    if (m_supplyLabel) {
        m_supplyLabel->setText(tr("Issued: %1 SFR").arg(supply));
    }
    if (m_ledgerBalanceTotalLabel) {
        m_ledgerBalanceTotalLabel->setText(tr("Balances: %1 SFR").arg(formatSfrValue(ledgerBalanceTotal)));
    }
    if (m_supplyDifferenceLabel) {
        m_supplyDifferenceLabel->setText(tr("Difference: %1 SFR").arg(formatSfrValue(supplyDifference)));
    }
    if (m_userCountLabel) {
        m_userCountLabel->setText(tr("Users: %1").arg(userCount));
    }
    if (m_blockCountLabel) {
        QString displayedBlockCount = blockCount.isEmpty() ? latestBlock : blockCount;
        m_blockCountLabel->setText(tr("Blocks: %1").arg(displayedBlockCount));
    }
    if (m_latestBlockRecordCountLabel) {
        QString displayedRecordCount = latestBlockRecordCount.isEmpty() ? tr("-") : latestBlockRecordCount;
        m_latestBlockRecordCountLabel->setText(tr("Latest records: %1").arg(displayedRecordCount));
    }

    if (m_receiveAddressLabel && !publicKey.isEmpty()) {
        m_receiveAddressLabel->setText(publicKey);
    }
}

void MainWindow::applyWalletAccounts(const QString &json)
{
    if (!m_accountCombo) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return;
    }
    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        return;
    }

    QString activeWalletId = object.value("active_wallet_id").toString();
    QJsonArray accounts = object.value("accounts").toArray();
    QString previousWalletId = m_accountCombo->currentData().toString();

    m_loadingAccounts = true;
    m_accountCombo->blockSignals(true);
    m_accountCombo->clear();
    int activeIndex = -1;
    int previousIndex = -1;
    for (int i = 0; i < accounts.size(); ++i) {
        QJsonObject account = accounts.at(i).toObject();
        QString walletId = account.value("wallet_id").toString();
        QString publicKey = account.value("public_key").toString();
        QString publicName = account.value("public_name").toString();
        QString label = account.value("label").toString();
        QString balance = account.value("balance").toString();
        QString displayName = publicName.isEmpty() ? (label.isEmpty() ? tr("Account") : label) : publicName;
        QString display = tr("%1 (%2) - %3 SFR").arg(displayName).arg(shortAddress(publicKey)).arg(formatSfrValue(balance));
        m_accountCombo->addItem(display, walletId);
        if (walletId == activeWalletId || account.value("active").toString() == "yes") {
            activeIndex = i;
        }
        if (walletId == previousWalletId) {
            previousIndex = i;
        }
    }

    if (activeIndex >= 0) {
        m_accountCombo->setCurrentIndex(activeIndex);
    } else if (previousIndex >= 0) {
        m_accountCombo->setCurrentIndex(previousIndex);
    }
    m_accountCombo->blockSignals(false);
    m_loadingAccounts = false;
}

void MainWindow::applyWalletHistory(const QString &json)
{
    if (!m_historyTable) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_historyTable->setRowCount(0);
        m_walletHistoryRecords = QJsonArray();
        m_historyPage = 0;
        // updateHistoryChart();
        appendHistory(tr("-"), tr("Error"), tr("Wallet history"), tr("-"), tr("Invalid response"));
        if (m_historyPageLabel) {
            m_historyPageLabel->setText(tr("Page 1 of 1 (0 records)"));
        }
        if (m_historyPrevButton) {
            m_historyPrevButton->setEnabled(false);
        }
        if (m_historyNextButton) {
            m_historyNextButton->setEnabled(false);
        }
        m_historyTable->resizeRowsToContents();
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        m_historyTable->setRowCount(0);
        m_walletHistoryRecords = QJsonArray();
        m_historyPage = 0;
        // updateHistoryChart();
        appendHistory(tr("-"), tr("Status"), tr("Wallet history"), tr("-"), object.value("status").toString());
        if (m_historyPageLabel) {
            m_historyPageLabel->setText(tr("Page 1 of 1 (0 records)"));
        }
        if (m_historyPrevButton) {
            m_historyPrevButton->setEnabled(false);
        }
        if (m_historyNextButton) {
            m_historyNextButton->setEnabled(false);
        }
        m_historyTable->resizeRowsToContents();
        return;
    }

    QJsonArray records = object.value("records").toArray();
    m_walletHistoryRecords = records;
    if (m_historyPage * 50 >= records.size()) {
        m_historyPage = 0;
    }
    // updateHistoryChart();
    renderHistoryPage();
}

void MainWindow::applyMempool(const QString &json)
{
    if (!m_mempoolTable) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_mempoolTable->setRowCount(0);
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        m_mempoolTable->setRowCount(0);
        return;
    }

    QJsonArray records = object.value("records").toArray();
    m_mempoolTable->setRowCount(0);
    for (int i = 0; i < records.size(); ++i) {
        QJsonObject record = records.at(i).toObject();
        QString type = record.value("type").toString();
        QString from = record.value("from_key").toString();
        QString to = record.value("to_key").toString();
        QString member = record.value("member_key").toString();
        QString name = record.value("name").toString();
        QString value = record.value("value").toString();
        QString amount = record.value("amount").toString();
        QString fee = record.value("fee").toString();
        QString hash = record.value("hash").toString();

        QString timeValue = record.value("time").toString();
        bool timeOk = false;
        qint64 epoch = timeValue.toLongLong(&timeOk);
        QString timeDisplay = timeValue;
        if (timeOk && epoch > 0) {
            timeDisplay = QDateTime::fromSecsSinceEpoch(epoch).toString("yyyy-MM-dd HH:mm");
        }
        if (timeDisplay.isEmpty()) {
            timeDisplay = tr("-");
        }

        QString fromDisplay = !member.isEmpty() ? shortAddress(member) : shortAddress(from);
        QString toDisplay = shortAddress(to);
        if (type == "JOIN_NETWORK" || type == "UPDATE_NAME") {
            toDisplay = name.isEmpty() ? tr("-") : tr("name \"%1\"").arg(name);
        } else if (type == "VOTE") {
            toDisplay = value.isEmpty() ? name : tr("%1: %2").arg(name).arg(value);
        }
        if (fromDisplay.isEmpty()) {
            fromDisplay = tr("-");
        }
        if (toDisplay.isEmpty()) {
            toDisplay = tr("-");
        }

        QString amountDisplay = amount.isEmpty() ? tr("-") : tr("%1 SFR").arg(amount);
        QString feeDisplay = fee.isEmpty() ? tr("-") : tr("%1 SFR").arg(fee);

        int row = m_mempoolTable->rowCount();
        m_mempoolTable->insertRow(row);
        QStringList values;
        values << record.value("index").toString(QString::number(i))
               << type
               << timeDisplay
               << fromDisplay
               << toDisplay
               << amountDisplay
               << feeDisplay
               << shortAddress(hash);
        for (int column = 0; column < values.size(); ++column) {
            m_mempoolTable->setItem(row, column, new QTableWidgetItem(values.at(column)));
        }
    }
    m_mempoolTable->resizeRowsToContents();
}

void MainWindow::renderBlockchainPage()
{
    if (!m_blockchainTable) {
        return;
    }

    const int pageSize = 10;
    int totalBlocks = m_blockchainBlocks.size();
    int pageCount = totalBlocks > 0 ? ((totalBlocks - 1) / pageSize) + 1 : 1;
    if (m_blockchainPage < 0) {
        m_blockchainPage = 0;
    }
    if (m_blockchainPage >= pageCount) {
        m_blockchainPage = pageCount - 1;
    }

    QString selectedKey = selectedBlockchainBlockKey();
    m_blockchainTable->setUpdatesEnabled(false);
    m_blockchainTable->setRowCount(0);
    if (totalBlocks == 0) {
        int row = m_blockchainTable->rowCount();
        m_blockchainTable->insertRow(row);
        m_blockchainTable->setItem(row, 0, new QTableWidgetItem(tr("Unknown")));
        m_blockchainTable->setItem(row, 1, new QTableWidgetItem(tr("-")));
        m_blockchainTable->setItem(row, 2, new QTableWidgetItem(tr("-")));
        m_blockchainTable->setItem(row, 3, new QTableWidgetItem(tr("-")));
        m_blockchainTable->setItem(row, 4, new QTableWidgetItem(tr("No blocks")));
        m_blockchainTable->setItem(row, 5, new QTableWidgetItem(tr("-")));
        m_blockchainTable->setItem(row, 6, new QTableWidgetItem(tr("-")));
    } else {
        int start = m_blockchainPage * pageSize;
        int end = start + pageSize;
        if (end > totalBlocks) {
            end = totalBlocks;
        }

        for (int i = start; i < end; ++i) {
            QJsonObject block = m_blockchainBlocks.at(i).toObject();
            QString blockKey = blockchainBlockKey(block);
            QString number = block.value("number").toString();
            QString networkStatus = block.value("network_status").toString();
            QString peerHash = block.value("peer_hash").toString();
            QString peerUrl = block.value("peer_url").toString();
            QString creatorKey = block.value("creator_key").toString();
            QString creatorName = block.value("creator_name").toString();
            QString hash = block.value("hash").toString();
            QString previous = block.value("previous_block_id").toString();
            QString previousHash = block.value("previous_hash").toString();
            QString recordsMerkleRoot = block.value("records_merkle_root").toString();

            QString timeValue = block.value("time").toString();
            bool timeOk = false;
            qint64 epoch = timeValue.toLongLong(&timeOk);
            QString timeDisplay = timeValue;
            if (timeOk && epoch > 0) {
                timeDisplay = QDateTime::fromSecsSinceEpoch(epoch).toString("yyyy-MM-dd HH:mm");
            }
            if (timeDisplay.isEmpty()) {
                timeDisplay = tr("-");
            }

            QString networkDisplay = tr("Unknown");
            QColor rowColor("#ffffff");
            if (networkStatus == "match") {
                networkDisplay = tr("Match");
                rowColor = QColor("#edf8f1");
            } else if (networkStatus == "mismatch") {
                networkDisplay = tr("Mismatch");
                rowColor = QColor("#fdecea");
            } else if (networkStatus == "ahead") {
                networkDisplay = tr("Ahead");
                rowColor = QColor("#eef4fb");
            }

            QStringList recordLines;
            QString searchBlob = number + QString(" ") + networkStatus + QString(" ") + peerHash + QString(" ") + peerUrl + QString(" ") +
                                 timeValue + QString(" ") + creatorKey + QString(" ") + creatorName + QString(" ") + hash + QString(" ") +
                                 previous + QString(" ") + previousHash + QString(" ") + recordsMerkleRoot;
            QJsonArray records = block.value("records").toArray();
            for (int r = 0; r < records.size(); ++r) {
                QJsonObject record = records.at(r).toObject();
                QString type = record.value("type").toString();
                QString amount = record.value("amount").toString();
                QString fee = record.value("fee").toString();
                QString fromKey = record.value("from_key").toString();
                QString fromName = record.value("from_name").toString();
                QString toKey = record.value("to_key").toString();
                QString toName = record.value("to_name").toString();
                QString name = record.value("name").toString();
                QString value = record.value("value").toString();
                QString recordHash = record.value("hash").toString();

                QString line = tr("#%1 %2").arg(record.value("index").toString(QString::number(r))).arg(type);
                if (type == "TRANSFER_CURRENCY" || type == "ISSUE_CURRENCY") {
                    line += tr(" %1 SFR").arg(amount);
                    line += tr(" from %1").arg(blockchainAccountLabel(fromName, fromKey));
                    line += tr(" to %1").arg(blockchainAccountLabel(toName, toKey));
                } else if (type == "JOIN_NETWORK" || type == "UPDATE_NAME") {
                    line += tr(" %1").arg(blockchainAccountLabel(fromName, fromKey));
                    if (!name.isEmpty()) {
                        line += tr(" name \"%1\"").arg(name);
                    }
                } else if (type == "VOTE") {
                    line += tr(" %1").arg(blockchainAccountLabel(fromName, fromKey));
                    if (!name.isEmpty() || !value.isEmpty()) {
                        line += tr(" %1=%2").arg(name).arg(value);
                    }
                } else if (!fromKey.isEmpty() || !toKey.isEmpty()) {
                    line += tr(" from %1").arg(blockchainAccountLabel(fromName, fromKey));
                    if (!toKey.isEmpty()) {
                        line += tr(" to %1").arg(blockchainAccountLabel(toName, toKey));
                    }
                }
                if (fee != "0" && !fee.isEmpty()) {
                    line += tr(" fee %1").arg(fee);
                }
                recordLines << line;
                searchBlob += QString(" ") + type + QString(" ") + amount + QString(" ") + fee + QString(" ") +
                              fromKey + QString(" ") + fromName + QString(" ") + toKey + QString(" ") + toName + QString(" ") +
                              name + QString(" ") + value + QString(" ") + recordHash;
            }

            QString recordsDisplay = recordLines.isEmpty() ? tr("No records") : recordLines.join("\n");
            int row = m_blockchainTable->rowCount();
            m_blockchainTable->insertRow(row);
            QStringList values;
            values << networkDisplay
                   << number
                   << timeDisplay
                   << blockchainAccountLabel(creatorName, creatorKey)
                   << recordsDisplay
                   << shortAddress(hash)
                   << tr("%1\n%2").arg(previous).arg(shortAddress(previousHash));
            for (int column = 0; column < values.size(); ++column) {
                QTableWidgetItem *item = new QTableWidgetItem(values.at(column));
                item->setData(Qt::UserRole, searchBlob);
                item->setData(Qt::UserRole + 1, blockKey);
                item->setBackground(rowColor);
                if (column == 0) {
                    QString tip = tr("Local hash: %1").arg(hash);
                    if (!recordsMerkleRoot.isEmpty()) {
                        tip += tr("\nRecords Merkle root: %1").arg(recordsMerkleRoot);
                    }
                    if (!peerHash.isEmpty()) {
                        tip += tr("\nPeer hash: %1").arg(peerHash);
                    }
                    if (!peerUrl.isEmpty()) {
                        tip += tr("\nPeer: %1").arg(peerUrl);
                    }
                    item->setToolTip(tip);
                }
                if (column == 4) {
                    item->setToolTip(recordsDisplay);
                }
                m_blockchainTable->setItem(row, column, item);
            }
        }
    }
    m_blockchainTable->resizeRowsToContents();
    m_blockchainTable->setUpdatesEnabled(true);

    if (m_blockchainPageLabel) {
        m_blockchainPageLabel->setText(tr("Page %1 of %2 (%3 blocks)")
                                       .arg(m_blockchainPage + 1)
                                       .arg(pageCount)
                                       .arg(totalBlocks));
    }
    if (m_blockchainPrevButton) {
        m_blockchainPrevButton->setEnabled(m_blockchainPage > 0);
    }
    if (m_blockchainNextButton) {
        m_blockchainNextButton->setEnabled(m_blockchainPage + 1 < pageCount);
    }
    filterBlockchainBlocks(m_blockchainSearchEdit ? m_blockchainSearchEdit->text() : QString());
    restoreBlockchainSelection(selectedKey);
}

void MainWindow::applyBlockchain(const QString &json)
{
    if (!m_blockchainTable) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_blockchainTable->setRowCount(0);
        m_blockchainBlocks = QJsonArray();
        m_blockchainPage = 0;
        renderBlockchainPage();
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        m_blockchainTable->setRowCount(0);
        m_blockchainBlocks = QJsonArray();
        m_blockchainPage = 0;
        renderBlockchainPage();
        return;
    }

    QString selectedKey = selectedBlockchainBlockKey();
    m_blockchainBlocks = object.value("blocks").toArray();
    if (!selectedKey.isEmpty()) {
        const int pageSize = 10;
        for (int i = 0; i < m_blockchainBlocks.size(); ++i) {
            if (blockchainBlockKey(m_blockchainBlocks.at(i).toObject()) == selectedKey) {
                m_blockchainPage = i / pageSize;
                break;
            }
        }
    }
    if (m_blockchainPage * 10 >= m_blockchainBlocks.size()) {
        m_blockchainPage = 0;
    }
    renderBlockchainPage();
}

void MainWindow::applyPeers(const QString &json)
{
    if (!m_peersTable || !m_peerMap) {
        return;
    }

    QJsonParseError parseError;
    QJsonDocument document = QJsonDocument::fromJson(json.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_peersTable->setRowCount(0);
        m_peerMap->setPeers(QStringList(), QStringList());
        return;
    }

    QJsonArray peers = document.object().value("peers").toArray();
    QStringList graphLabels;
    QStringList graphStates;
    m_peersTable->setRowCount(0);
    for (int i = 0; i < peers.size(); ++i) {
        QJsonObject peer = peers.at(i).toObject();
        QString url = peer.value("url").toString();
        QString reachable = peer.value("reachable").toString();
        QString genesis = peer.value("genesis_match").toString();
        QString latestBlock = peer.value("latest_block_id").toString();
        QString score = peer.value("score").toString();

        QString state = "offline";
        if (reachable == "yes" && genesis == "yes") {
            state = "reachable";
        } else if (reachable == "yes" && genesis != "yes") {
            state = "mismatch";
        }
        graphLabels << shortPeerLabel(url);
        graphStates << state;

        int row = m_peersTable->rowCount();
        m_peersTable->insertRow(row);
        QStringList values;
        values << (url.isEmpty() ? tr("-") : url)
               << (reachable.isEmpty() ? tr("-") : reachable)
               << (genesis.isEmpty() ? tr("-") : genesis)
               << (latestBlock.isEmpty() ? tr("-") : latestBlock)
               << (score.isEmpty() ? tr("-") : score);
        for (int column = 0; column < values.size(); ++column) {
            QTableWidgetItem *item = new QTableWidgetItem(values.at(column));
            if (state == "reachable") {
                item->setForeground(QColor("#18735d"));
            } else if (state == "mismatch") {
                item->setForeground(QColor("#9a5b00"));
            } else {
                item->setForeground(QColor("#6b7e84"));
            }
            m_peersTable->setItem(row, column, item);
        }
    }

    QString centerLabel = m_publicName.isEmpty() ? shortAddress(m_publicKey) : m_publicName;
    m_peerMap->setCenterLabel(centerLabel.isEmpty() ? tr("This wallet") : centerLabel);
    m_peerMap->setPeers(graphLabels, graphStates);
    m_peersTable->resizeRowsToContents();
}

void MainWindow::signIn()
{
    QString password = m_passwordEdit->text();
    if (password.isEmpty()) {
        m_loginMessage->setText(passwordHashExists() ? tr("Enter your password.") : tr("Choose a password."));
        return;
    }

    QString hash = passwordHashFor(password);
    if (passwordHashExists()) {
        QString storedHash = readPasswordHash();
        if (storedHash.compare(hash, Qt::CaseInsensitive) != 0) {
            m_loginMessage->setText(tr("Incorrect password."));
            m_passwordEdit->selectAll();
            return;
        }
        openWalletShell(tr("Wallet unlocked"));
        return;
    }

    if (!savePasswordHash(hash)) {
        m_loginMessage->setText(tr("Could not save password hash."));
        return;
    }

    openWalletShell(tr("Wallet unlocked"));
}

void MainWindow::skipSignIn()
{
    if (passwordHashExists()) {
        m_loginMessage->setText(tr("Password required."));
        return;
    }
    openWalletShell(tr("Wallet unlocked without password"));
}

void MainWindow::openWalletShell(const QString &statusText)
{
    m_loginMessage->clear();
    m_userLabel->setText(statusText);
    m_passwordEdit->clear();
    m_rootStack->setCurrentIndex(1);
    m_backendStartBlocked = false;
    m_backendLockDetected = false;
    ensureBackendRunning();
    refreshWalletStatus();
    m_statusTimer->start();
}

void MainWindow::signOut()
{
    m_statusTimer->stop();
    stopTerminal();
    m_rootStack->setCurrentIndex(0);
    if (m_loginHintLabel) {
        bool hasPassword = passwordHashExists();
        m_loginHintLabel->setText(hasPassword ? tr("Enter your wallet password to continue.") :
                                                tr("No local password is set yet."));
    }
    if (m_loginPrimaryButton) {
        m_loginPrimaryButton->setText(passwordHashExists() ? tr("Unlock Wallet") : tr("Set Password"));
    }
    if (m_loginSkipButton) {
        m_loginSkipButton->setVisible(!passwordHashExists());
    }
    m_passwordEdit->setFocus();
}

void MainWindow::showBalance()
{
    m_contentStack->setCurrentIndex(0);
    setActiveNav(m_balanceButton);
    refreshWalletStatus();
}

void MainWindow::showSend()
{
    m_contentStack->setCurrentIndex(1);
    setActiveNav(m_sendButton);
}

void MainWindow::showReceive()
{
    m_contentStack->setCurrentIndex(2);
    setActiveNav(m_receiveButton);
}

void MainWindow::showContacts()
{
    m_contentStack->setCurrentIndex(3);
    setActiveNav(m_contactsButton);
    refreshWalletStatus();
}

void MainWindow::showHistory()
{
    m_contentStack->setCurrentIndex(4);
    setActiveNav(m_historyButton);
    refreshWalletStatus();
}

void MainWindow::previousHistoryPage()
{
    if (m_historyPage > 0) {
        --m_historyPage;
        renderHistoryPage();
    }
}

void MainWindow::nextHistoryPage()
{
    const int pageSize = 50;
    int pageCount = m_walletHistoryRecords.size() > 0 ? ((m_walletHistoryRecords.size() - 1) / pageSize) + 1 : 1;
    if (m_historyPage + 1 < pageCount) {
        ++m_historyPage;
        renderHistoryPage();
    }
}

void MainWindow::showMempool()
{
    m_contentStack->setCurrentIndex(5);
    setActiveNav(m_mempoolButton);
    refreshWalletStatus();
}

void MainWindow::showBlockchain()
{
    m_contentStack->setCurrentIndex(6);
    setActiveNav(m_blockchainButton);
    refreshWalletStatus();
}

void MainWindow::previousBlockchainPage()
{
    if (m_blockchainPage > 0) {
        --m_blockchainPage;
        renderBlockchainPage();
    }
}

void MainWindow::nextBlockchainPage()
{
    const int pageSize = 10;
    int pageCount = m_blockchainBlocks.size() > 0 ? ((m_blockchainBlocks.size() - 1) / pageSize) + 1 : 1;
    if (m_blockchainPage + 1 < pageCount) {
        ++m_blockchainPage;
        renderBlockchainPage();
    }
}

void MainWindow::showPeers()
{
    m_contentStack->setCurrentIndex(7);
    setActiveNav(m_peersButton);
    refreshWalletStatus();
}

void MainWindow::showTerminal()
{
    m_contentStack->setCurrentIndex(8);
    setActiveNav(m_terminalButton);
}

void MainWindow::showOptions()
{
    m_contentStack->setCurrentIndex(9);
    setActiveNav(m_optionsButton);
}

void MainWindow::copyReceiveAddress()
{
    QApplication::clipboard()->setText(receiveAddress());
    QMessageBox::information(this, tr("Safire"), tr("Address copied."));
}

void MainWindow::filterNetworkUsers(const QString &text)
{
    if (!m_networkUsersTable) {
        return;
    }

    QString selectedAddress = selectedNetworkUserAddress();
    QString needle = text.trimmed().toLower();
    for (int row = 0; row < m_networkUsersTable->rowCount(); ++row) {
        QString haystack;
        for (int column = 0; column < m_networkUsersTable->columnCount(); ++column) {
            QTableWidgetItem *item = m_networkUsersTable->item(row, column);
            if (item) {
                haystack += item->text().toLower() + QString(" ");
            }
        }
        m_networkUsersTable->setRowHidden(row, !needle.isEmpty() && !haystack.contains(needle));
    }
    restoreNetworkUserSelection(selectedAddress);
}

void MainWindow::filterBlockchainBlocks(const QString &text)
{
    if (!m_blockchainTable) {
        return;
    }

    QString selectedKey = selectedBlockchainBlockKey();
    QString needle = text.trimmed().toLower();
    for (int row = 0; row < m_blockchainTable->rowCount(); ++row) {
        QString haystack;
        for (int column = 0; column < m_blockchainTable->columnCount(); ++column) {
            QTableWidgetItem *item = m_blockchainTable->item(row, column);
            if (item) {
                haystack += item->text().toLower() + QString(" ");
                haystack += item->data(Qt::UserRole).toString().toLower() + QString(" ");
            }
        }
        m_blockchainTable->setRowHidden(row, !needle.isEmpty() && !haystack.contains(needle));
    }
    restoreBlockchainSelection(selectedKey);
}

void MainWindow::updateHistoryChart()
{
    return;

    if (!m_historyChart) {
        return;
    }

    int days = 30;
    if (m_historyRangeCombo) {
        days = m_historyRangeCombo->currentData().toInt();
    }

    qint64 newestEpoch = 0;
    for (int i = 0; i < m_walletHistoryRecords.size(); ++i) {
        QJsonObject record = m_walletHistoryRecords.at(i).toObject();
        bool timeOk = false;
        qint64 epoch = record.value("time").toString().toLongLong(&timeOk);
        if (timeOk && epoch > newestEpoch) {
            newestEpoch = epoch;
        }
    }
    if (newestEpoch <= 0) {
        newestEpoch = QDateTime::currentSecsSinceEpoch();
    }

    qint64 cutoff = days > 0 ? newestEpoch - (static_cast<qint64>(days) * 86400) : 0;
    QVector<QPointF> received;
    QVector<QPointF> outgoing;
    double receivedTotal = 0.0;
    double outgoingTotal = 0.0;

    for (int i = 0; i < m_walletHistoryRecords.size(); ++i) {
        QJsonObject record = m_walletHistoryRecords.at(i).toObject();
        bool timeOk = false;
        qint64 epoch = record.value("time").toString().toLongLong(&timeOk);
        if (!timeOk || epoch <= 0 || (cutoff > 0 && epoch < cutoff)) {
            continue;
        }

        bool netOk = false;
        double netAmount = record.value("net").toString().toDouble(&netOk);
        if (!netOk || netAmount == 0.0) {
            continue;
        }

        if (netAmount > 0.0) {
            receivedTotal += netAmount;
            received.append(QPointF(static_cast<double>(epoch), receivedTotal));
        } else {
            outgoingTotal += netAmount;
            outgoing.append(QPointF(static_cast<double>(epoch), outgoingTotal));
        }
    }

    m_historyChart->setSeries(received, outgoing, receivedTotal, outgoingTotal);
}

void MainWindow::addSelectedNetworkUserToContacts()
{
    if (!m_networkUsersTable) {
        return;
    }

    int row = m_networkUsersTable->currentRow();
    if (row < 0 || row >= m_networkUsersTable->rowCount() || m_networkUsersTable->isRowHidden(row)) {
        QList<QTableWidgetItem *> selectedItems = m_networkUsersTable->selectedItems();
        if (!selectedItems.isEmpty()) {
            row = selectedItems.first()->row();
        }
    }
    if (row < 0 || row >= m_networkUsersTable->rowCount() || m_networkUsersTable->isRowHidden(row)) {
        QMessageBox::information(this, tr("Safire"), tr("Select a network user first."));
        return;
    }

    QTableWidgetItem *nameItem = m_networkUsersTable->item(row, 0);
    QTableWidgetItem *addressItem = m_networkUsersTable->item(row, 1);
    QString name = nameItem ? nameItem->text() : QString();
    QString address = addressItem ? addressItem->data(Qt::UserRole).toString() : QString();
    if (address.isEmpty() && addressItem) {
        address = addressItem->text();
    }
    if (name == tr("-")) {
        name.clear();
    }

    int before = m_contactsTable ? m_contactsTable->rowCount() : 0;
    appendContact(name, address);
    if (m_contactsTable && m_contactsTable->rowCount() > before) {
        saveContacts();
        refreshContactDropdown();
    }
}

void MainWindow::removeSelectedContact()
{
    if (!m_contactsTable) {
        return;
    }

    QList<QTableWidgetItem *> selectedItems = m_contactsTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("Safire"), tr("Select a contact first."));
        return;
    }

    m_contactsTable->removeRow(selectedItems.first()->row());
    saveContacts();
    refreshContactDropdown();
}

void MainWindow::useSelectedContactForSend()
{
    if (!m_contactsTable) {
        return;
    }

    QList<QTableWidgetItem *> selectedItems = m_contactsTable->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, tr("Safire"), tr("Select a contact first."));
        return;
    }

    int row = selectedItems.first()->row();
    QTableWidgetItem *addressItem = m_contactsTable->item(row, 1);
    if (addressItem && m_sendToEdit) {
        m_sendToEdit->setText(addressItem->text());
    }
    showSend();
}

void MainWindow::setSendRecipientFromContact(int index)
{
    if (!m_sendContactCombo || !m_sendToEdit || index <= 0) {
        return;
    }

    QString address = m_sendContactCombo->itemData(index).toString();
    if (!address.isEmpty()) {
        m_sendToEdit->setText(address);
    }
}

void MainWindow::submitPayment()
{
    QString recipient = m_sendToEdit->text().trimmed();
    QString amount = m_sendAmountEdit->text().trimmed();
    QString fee = m_sendFeeEdit ? m_sendFeeEdit->text().trimmed() : QString();
    if (recipient.isEmpty() || amount.isEmpty()) {
        QMessageBox::warning(this, tr("Safire"), tr("Enter a recipient address and amount."));
        return;
    }

    bool amountOk = false;
    double amountValue = amount.toDouble(&amountOk);
    if (!amountOk || amountValue <= 0.0) {
        QMessageBox::warning(this, tr("Safire"), tr("Enter an amount greater than zero."));
        return;
    }

    double feeValue = m_transactionFee;
    if (!fee.isEmpty()) {
        bool feeOk = false;
        feeValue = fee.toDouble(&feeOk);
        if (!feeOk || feeValue < 0.0) {
            QMessageBox::warning(this, tr("Safire"), tr("Enter a transfer fee of zero or greater."));
            return;
        }
    }

    double totalDebit = amountValue + feeValue;
    QString confirmMessage = tr("Send %1 SFR to:\n%2\n\nFee: %3 SFR\nTotal debit: %4 SFR")
        .arg(QString::number(amountValue, 'f', 4))
        .arg(recipient)
        .arg(QString::number(feeValue, 'f', 4))
        .arg(QString::number(totalDebit, 'f', 4));
    int answer = QMessageBox::question(this, tr("Confirm Payment"), confirmMessage, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) {
        return;
    }

    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/send").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("recipient", recipient);
    form.addQueryItem("amount", amount);
    form.addQueryItem("fee", QString::number(feeValue, 'f', 8));
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::setWalletName()
{
    QString currentName = m_publicName;
    bool ok = false;
    QString name = QInputDialog::getText(this,
                                         tr("Set Public Name"),
                                         tr("Public name"),
                                         QLineEdit::Normal,
                                         currentName,
                                         &ok).trimmed();
    if (!ok) {
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Safire"), tr("Enter a public name."));
        return;
    }

    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/setname").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("name", name);
    if (m_accountCombo && m_accountCombo->currentIndex() >= 0) {
        QString walletId = m_accountCombo->currentData().toString();
        if (!walletId.isEmpty()) {
            form.addQueryItem("wallet_id", walletId);
        }
    }
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::joinNetwork()
{
    QString currentName = m_publicName;
    bool ok = false;
    QString name = QInputDialog::getText(this,
                                         tr("Join Network"),
                                         tr("Public name"),
                                         QLineEdit::Normal,
                                         currentName,
                                         &ok).trimmed();
    if (!ok) {
        return;
    }
    if (name.isEmpty()) {
        QMessageBox::warning(this, tr("Safire"), tr("Enter a public name."));
        return;
    }

    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/join").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("name", name);
    if (m_accountCombo && m_accountCombo->currentIndex() >= 0) {
        QString walletId = m_accountCombo->currentData().toString();
        if (!walletId.isEmpty()) {
            form.addQueryItem("wallet_id", walletId);
        }
    }
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::addWalletAccount()
{
    bool ok = false;
    QString label = QInputDialog::getText(this,
                                          tr("Add Account"),
                                          tr("Local account label"),
                                          QLineEdit::Normal,
                                          tr("Account"),
                                          &ok).trimmed();
    if (!ok) {
        return;
    }
    if (label.isEmpty()) {
        label = tr("Account");
    }

    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/accounts/create").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("label", label);
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::switchWalletAccount(int index)
{
    if (m_loadingAccounts || index < 0 || !m_accountCombo) {
        return;
    }
    QString walletId = m_accountCombo->itemData(index).toString();
    if (walletId.isEmpty() || walletId == m_publicKey) {
        return;
    }
    if (!ensureBackendRunning()) {
        QMessageBox::warning(this, tr("Safire"), tr("Backend is unavailable."));
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/accounts/active").arg(m_backendPort));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery form;
    form.addQueryItem("wallet_id", walletId);
    m_networkManager->post(request, form.query(QUrl::FullyEncoded).toUtf8());
}

void MainWindow::startTerminal()
{
    m_backendStartBlocked = false;
    m_backendLockDetected = false;
    if (ensureBackendRunning() && m_terminalInput) {
        m_terminalInput->setFocus();
    }
}

void MainWindow::stopTerminal()
{
    if (!m_terminalProcess || m_terminalProcess->state() == QProcess::NotRunning) {
        return;
    }

    appendTerminalText(tr("\nStopping console backend...\n"));
    m_terminalProcess->write("quit\n");
    if (!m_terminalProcess->waitForFinished(2500)) {
        m_terminalProcess->terminate();
        if (!m_terminalProcess->waitForFinished(1500)) {
            m_terminalProcess->kill();
            m_terminalProcess->waitForFinished(1000);
        }
    }
}

void MainWindow::sendTerminalCommand()
{
    if (!m_terminalInput) {
        return;
    }

    QString command = m_terminalInput->text();
    if (command.trimmed().isEmpty()) {
        return;
    }

    if (!m_terminalProcess || m_terminalProcess->state() == QProcess::NotRunning) {
        startTerminal();
    }

    if (!m_terminalProcess || m_terminalProcess->state() == QProcess::NotRunning) {
        appendTerminalText(tr("\nConsole backend is not running.\n"));
        return;
    }

    appendTerminalText(QString(">") + command + "\n");
    QByteArray bytes = command.toUtf8();
    bytes.append('\n');
    m_terminalProcess->write(bytes);
    m_terminalInput->clear();
}

void MainWindow::readTerminalOutput()
{
    if (!m_terminalProcess) {
        return;
    }

    QByteArray bytes = m_terminalProcess->readAllStandardOutput();
    QString text = QString::fromLocal8Bit(bytes);
    appendTerminalText(text);
    if (text.contains("Resource temporarily unavailable") ||
        text.contains("Another Safire process")) {
        m_backendLockDetected = true;
        m_backendStartBlocked = true;
        if (m_terminalStatusLabel) {
            m_terminalStatusLabel->setText(tr("Database locked"));
        }
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: database locked"));
        }
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: close the other Safire app or node"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 100);
            m_syncProgressBar->setValue(0);
        }
    }
}

void MainWindow::terminalFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_terminalProcess) {
        QByteArray bytes = m_terminalProcess->readAllStandardOutput();
        appendTerminalText(QString::fromLocal8Bit(bytes));
    }
    appendTerminalText(tr("\nConsole backend stopped with exit code %1.\n").arg(exitCode));
    if (m_backendRestartPending) {
        m_backendRestartPending = false;
        m_backendStartBlocked = false;
        m_backendLockDetected = false;
        if (m_terminalStatusLabel) {
            m_terminalStatusLabel->setText(tr("Restarting"));
        }
        if (m_networkLabel) {
            m_networkLabel->setText(tr("Backend: restarting"));
        }
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: restarting backend"));
        }
        if (m_terminalStartButton) {
            m_terminalStartButton->setEnabled(false);
        }
        if (m_terminalStopButton) {
            m_terminalStopButton->setEnabled(false);
        }
        QTimer::singleShot(300, this, SLOT(startTerminal()));
        QTimer::singleShot(1000, this, SLOT(refreshWalletStatus()));
        return;
    }
    if (exitCode != 0) {
        m_backendStartBlocked = true;
    }
    if (m_terminalStatusLabel) {
        m_terminalStatusLabel->setText(m_backendLockDetected ? tr("Database locked") : tr("Stopped"));
    }
    if (m_terminalStartButton) {
        m_terminalStartButton->setEnabled(true);
    }
    if (m_terminalStopButton) {
        m_terminalStopButton->setEnabled(false);
    }
    if (m_networkLabel) {
        m_networkLabel->setText(m_backendLockDetected ? tr("Backend: database locked") : tr("Backend: stopped"));
    }
    if (m_balanceLabel) {
        m_balanceLabel->setText(tr("-"));
    }
    if (m_estimatedBalanceLabel) {
        m_estimatedBalanceLabel->setText(tr("Estimated: -"));
    }
    if (m_pendingBalanceLabel) {
        m_pendingBalanceLabel->setText(tr("Pending: none"));
    }
    if (m_backendLockDetected && m_syncLabel) {
        m_syncLabel->setText(tr("Sync: close the other Safire app or node"));
    }
}

void MainWindow::terminalError(QProcess::ProcessError error)
{
    Q_UNUSED(error);
    if (!m_terminalProcess) {
        return;
    }
    appendTerminalText(tr("\nConsole backend error: %1\n").arg(m_terminalProcess->errorString()));
    if (m_terminalStatusLabel) {
        m_terminalStatusLabel->setText(tr("Error"));
    }
    if (m_terminalStartButton) {
        m_terminalStartButton->setEnabled(true);
    }
    if (m_terminalStopButton) {
        m_terminalStopButton->setEnabled(false);
    }
}

void MainWindow::refreshWalletStatus()
{
    if (!ensureBackendRunning()) {
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: backend unavailable"));
        }
        if (m_balanceLabel) {
            m_balanceLabel->setText(tr("-"));
        }
        if (m_estimatedBalanceLabel) {
            m_estimatedBalanceLabel->setText(tr("Estimated: -"));
        }
        if (m_pendingBalanceLabel) {
            m_pendingBalanceLabel->setText(tr("Pending: none"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 0);
        }
        return;
    }

    requestJson("/api/wallet/status");
    if (m_accountRefreshTicks <= 0) {
        requestJson("/api/wallet/accounts");
        m_accountRefreshTicks = 10;
    } else {
        --m_accountRefreshTicks;
    }

    if (!m_contentStack) {
        return;
    }

    int page = m_contentStack->currentIndex();
    if (page == 3) {
        requestJson("/api/network/users");
    } else if (page == 4) {
        requestJson("/api/wallet/history");
    } else if (page == 5) {
        requestJson("/api/mempool");
    } else if (page == 6) {
        requestJson("/api/blockchain/recent");
    } else if (page == 7) {
        requestJson("/api/peers");
    }
}

void MainWindow::requestJson(const QString &path)
{
    if (m_pendingRequests.contains(path)) {
        return;
    }

    m_pendingRequests.append(path);
    if (isPageDataPath(path) && !m_loadedDataPaths.contains(path)) {
        QTimer::singleShot(250, this, [this, path]() {
            if (m_pendingRequests.contains(path) && !m_loadedDataPaths.contains(path)) {
                setLoadingState(path, true);
            }
        });
    }
    QUrl url(QString("http://127.0.0.1:%1%2").arg(m_backendPort).arg(path));
    QNetworkRequest request(url);
    QNetworkReply *reply = m_networkManager->get(request);
    QPointer<QNetworkReply> guardedReply(reply);
    QTimer::singleShot(8000, this, [this, guardedReply, path]() {
        if (!guardedReply) {
            return;
        }
        if (!m_pendingRequests.contains(path)) {
            return;
        }
        m_pendingRequests.removeAll(path);
        setLoadingState(path, false);
        if (path == "/api/wallet/status" && m_syncLabel) {
            m_syncLabel->setText(tr("Sync: request timed out"));
        }
        if (guardedReply->isRunning()) {
            guardedReply->abort();
        }
    });
}

void MainWindow::handleWalletStatusReply(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    QByteArray body = reply->readAll();
    QString path = reply->url().path();
    m_pendingRequests.removeAll(path);
    setLoadingState(path, false);
    if (reply->error() != QNetworkReply::NoError) {
        if (path == "/api/wallet/send") {
            applySendPaymentResult(QString::fromUtf8(body), true);
            reply->deleteLater();
            return;
        }
        if (path == "/api/wallet/join") {
            applyJoinNetworkResult(QString::fromUtf8(body), true);
            reply->deleteLater();
            return;
        }
        if (path == "/api/wallet/setname") {
            applySetNameResult(QString::fromUtf8(body), true);
            reply->deleteLater();
            return;
        }
        if (path == "/api/wallet/accounts/create") {
            applyWalletAccountCreateResult(QString::fromUtf8(body), true);
            reply->deleteLater();
            return;
        }
        if (path == "/api/blockchain/reset") {
            applyBlockchainResetResult(QString::fromUtf8(body), true);
            reply->deleteLater();
            return;
        }
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: waiting for backend API"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 0);
        }
        reply->deleteLater();
        return;
    }

    if (isPageDataPath(path) && !m_loadedDataPaths.contains(path)) {
        m_loadedDataPaths.append(path);
    }

    if (path == "/api/wallet/history") {
        applyWalletHistory(QString::fromUtf8(body));
    } else if (path == "/api/wallet/accounts") {
        applyWalletAccounts(QString::fromUtf8(body));
    } else if (path == "/api/mempool") {
        applyMempool(QString::fromUtf8(body));
    } else if (path == "/api/blockchain/recent") {
        applyBlockchain(QString::fromUtf8(body));
    } else if (path == "/api/peers") {
        applyPeers(QString::fromUtf8(body));
    } else if (path == "/api/network/users") {
        applyNetworkUsers(QString::fromUtf8(body));
    } else if (path == "/api/wallet/send") {
        applySendPaymentResult(QString::fromUtf8(body), false);
    } else if (path == "/api/wallet/join") {
        applyJoinNetworkResult(QString::fromUtf8(body), false);
    } else if (path == "/api/wallet/setname") {
        applySetNameResult(QString::fromUtf8(body), false);
    } else if (path == "/api/wallet/accounts/create") {
        applyWalletAccountCreateResult(QString::fromUtf8(body), false);
    } else if (path == "/api/wallet/accounts/active") {
        applyWalletAccounts(QString::fromUtf8(body));
        requestJson("/api/wallet/status");
    } else if (path == "/api/blockchain/reset") {
        applyBlockchainResetResult(QString::fromUtf8(body), false);
    } else {
        applyWalletStatus(QString::fromUtf8(body));
    }
    reply->deleteLater();
}
