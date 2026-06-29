#include "mainwindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QCoreApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
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
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QTimer>
#include <QUrl>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextCursor>
#include <QTextEdit>
#include <QVBoxLayout>
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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_rootStack(new QStackedWidget(this)),
      m_contentStack(0),
      m_terminalProcess(0),
      m_statusTimer(new QTimer(this)),
      m_networkManager(new QNetworkAccessManager(this)),
      m_backendPort(4899),
      m_backendStartBlocked(false),
      m_userEdit(0),
      m_passwordEdit(0),
      m_loginMessage(0),
      m_userLabel(0),
      m_balanceLabel(0),
      m_networkLabel(0),
      m_syncLabel(0),
      m_syncProgressBar(0),
      m_peerLabel(0),
      m_supplyLabel(0),
      m_historyTable(0),
      m_sendToEdit(0),
      m_sendAmountEdit(0),
      m_sendMemoEdit(0),
      m_receiveAddressLabel(0),
      m_terminalOutput(0),
      m_terminalInput(0),
      m_terminalStatusLabel(0),
      m_terminalStartButton(0),
      m_terminalStopButton(0),
      m_balanceButton(0),
      m_sendButton(0),
      m_receiveButton(0),
      m_historyButton(0),
      m_terminalButton(0),
      m_optionsButton(0)
{
    setWindowTitle(tr("Safire Wallet"));
    resize(1120, 720);
    setMinimumSize(920, 620);

    setStyleSheet(
        "QWidget { color: #152328; font-family: 'Helvetica Neue', Arial; font-size: 14px; }"
        "#LoginCard, #SideBar, #Panel, #AccountCard { background: rgba(255, 255, 255, 235); border: 1px solid #dce7ea; border-radius: 8px; }"
        "#AppTitle { color: #0f2c34; font-size: 34px; font-weight: 700; }"
        "#HeroText { color: #41545a; font-size: 15px; line-height: 150%; }"
        "#SectionTitle { color: #12272d; font-size: 24px; font-weight: 700; }"
        "#SmallTitle { color: #253b42; font-size: 16px; font-weight: 700; }"
        "#Muted { color: #6b7e84; }"
        "#Balance { color: #08282f; font-size: 36px; font-weight: 700; }"
        "#AccountBalance { color: #0d343b; font-size: 22px; font-weight: 700; }"
        "#StatusGood { color: #18735d; font-weight: 700; }"
        "#Error { color: #b3261e; font-weight: 600; }"
        "#TerminalOutput { background: #0d171b; color: #d9f7ef; border: 1px solid #17343b; border-radius: 6px; font-family: Menlo, Consolas, monospace; font-size: 13px; padding: 10px; }"
        "QLineEdit, QTextEdit { background: #ffffff; border: 1px solid #cddadd; border-radius: 6px; padding: 10px; selection-background-color: #1b7f8a; }"
        "QLineEdit:focus, QTextEdit:focus { border-color: #1b7f8a; }"
        "QPushButton { border-radius: 6px; padding: 10px 14px; font-weight: 700; }"
        "QPushButton#PrimaryButton { background: #166b76; color: white; border: 1px solid #166b76; }"
        "QPushButton#PrimaryButton:hover { background: #0f5963; }"
        "QPushButton#SecondaryButton { background: #edf4f5; color: #17454d; border: 1px solid #c8d9dc; }"
        "QPushButton#SecondaryButton:hover { background: #e1eeee; }"
        "QPushButton#NavButton { background: transparent; color: #31525a; border: 1px solid transparent; text-align: left; padding: 11px 12px; }"
        "QPushButton#NavButton:hover { background: #eef6f7; }"
        "QPushButton#NavButton[active='true'] { background: #dceff1; color: #0f5963; border: 1px solid #b8d9dd; }"
        "QProgressBar { background: #edf4f5; border: 1px solid #c8d9dc; border-radius: 6px; height: 14px; text-align: center; color: #31525a; }"
        "QProgressBar::chunk { background: #166b76; border-radius: 5px; }"
        "QTableWidget { background: white; border: 1px solid #dce7ea; border-radius: 6px; gridline-color: #edf2f3; }"
        "QHeaderView::section { background: #edf4f5; color: #41545a; padding: 8px; border: 0; font-weight: 700; }"
    );

    m_rootStack->addWidget(createLoginPage());
    m_rootStack->addWidget(createShellPage());
    setCentralWidget(m_rootStack);

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
    intro->addWidget(makeLabel(tr("Local wallet access"), "SmallTitle"));
    intro->addWidget(makeLabel(tr("This first GUI keeps authentication local and mocked until the wallet core is connected."), "Muted"));
    intro->addStretch();

    QFrame *card = makePanel("LoginCard");
    card->setFixedWidth(380);
    QVBoxLayout *form = new QVBoxLayout(card);
    form->setContentsMargins(28, 28, 28, 28);
    form->setSpacing(14);

    form->addWidget(makeLabel(tr("Open Wallet"), "SectionTitle"));
    form->addWidget(makeLabel(tr("Sign in to continue."), "Muted"));

    m_userEdit = new QLineEdit;
    m_userEdit->setPlaceholderText(tr("User"));
    form->addWidget(m_userEdit);

    m_passwordEdit = new QLineEdit;
    m_passwordEdit->setPlaceholderText(tr("Password"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    form->addWidget(m_passwordEdit);

    m_loginMessage = makeLabel(QString(), "Error");
    m_loginMessage->setMinimumHeight(22);
    form->addWidget(m_loginMessage);

    QPushButton *signInButton = createPrimaryButton(tr("Sign In"));
    form->addWidget(signInButton);
    form->addWidget(makeLabel(tr("Prototype mode: any non-empty user and password will open the wallet."), "Muted"));
    form->addStretch();

    connect(signInButton, SIGNAL(clicked()), this, SLOT(signIn()));
    connect(m_passwordEdit, SIGNAL(returnPressed()), this, SLOT(signIn()));

    layout->addLayout(intro, 1);
    layout->addWidget(card);
    return page;
}

QWidget *MainWindow::createShellPage()
{
    AccentCanvas *page = new AccentCanvas;
    QHBoxLayout *layout = new QHBoxLayout(page);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(18);

    QFrame *sideBar = makePanel("SideBar");
    sideBar->setFixedWidth(238);
    QVBoxLayout *nav = new QVBoxLayout(sideBar);
    nav->setContentsMargins(18, 18, 18, 18);
    nav->setSpacing(10);

    nav->addWidget(makeLabel(tr("Safire"), "AppTitle"));
    m_userLabel = makeLabel(tr("Not signed in"), "Muted");
    nav->addWidget(m_userLabel);
    nav->addSpacing(16);

    m_balanceButton = createNavButton(tr("Accounts"));
    m_sendButton = createNavButton(tr("Send"));
    m_receiveButton = createNavButton(tr("Receive"));
    m_historyButton = createNavButton(tr("History"));
    m_terminalButton = createNavButton(tr("Terminal"));
    m_optionsButton = createNavButton(tr("Options"));

    nav->addWidget(m_balanceButton);
    nav->addWidget(m_sendButton);
    nav->addWidget(m_receiveButton);
    nav->addWidget(m_historyButton);
    nav->addWidget(m_terminalButton);
    nav->addWidget(m_optionsButton);
    nav->addStretch();

    QPushButton *signOutButton = createSecondaryButton(tr("Lock Wallet"));
    nav->addWidget(signOutButton);

    m_contentStack = new QStackedWidget;
    m_contentStack->addWidget(createBalancePage());
    m_contentStack->addWidget(createSendPage());
    m_contentStack->addWidget(createReceivePage());
    m_contentStack->addWidget(createHistoryPage());
    m_contentStack->addWidget(createTerminalPage());
    m_contentStack->addWidget(createOptionsPage());

    connect(m_balanceButton, SIGNAL(clicked()), this, SLOT(showBalance()));
    connect(m_sendButton, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(m_receiveButton, SIGNAL(clicked()), this, SLOT(showReceive()));
    connect(m_historyButton, SIGNAL(clicked()), this, SLOT(showHistory()));
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
    layout->setSpacing(16);

    QFrame *summary = makePanel("Panel");
    QGridLayout *summaryLayout = new QGridLayout(summary);
    summaryLayout->setContentsMargins(26, 24, 26, 24);
    summaryLayout->setSpacing(10);

    summaryLayout->addWidget(createSectionTitle(tr("Accounts")), 0, 0);
    summaryLayout->addWidget(makeLabel(tr("Available Balance"), "Muted"), 1, 0);
    m_balanceLabel = makeLabel(tr("0.9877 SFR"), "Balance");
    summaryLayout->addWidget(m_balanceLabel, 2, 0);
    m_networkLabel = makeLabel(tr("Network: not joined"), "StatusGood");
    summaryLayout->addWidget(m_networkLabel, 3, 0);
    m_syncLabel = makeLabel(tr("Sync: waiting for backend"), "Muted");
    summaryLayout->addWidget(m_syncLabel, 4, 0);
    m_syncProgressBar = new QProgressBar;
    m_syncProgressBar->setRange(0, 100);
    m_syncProgressBar->setValue(0);
    m_syncProgressBar->setTextVisible(false);
    summaryLayout->addWidget(m_syncProgressBar, 5, 0);
    m_peerLabel = makeLabel(tr("Peers: -"), "Muted");
    summaryLayout->addWidget(m_peerLabel, 6, 0);
    m_supplyLabel = makeLabel(tr("Supply: -"), "Muted");
    summaryLayout->addWidget(m_supplyLabel, 7, 0);

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *sendNow = createPrimaryButton(tr("Send"));
    QPushButton *receiveNow = createSecondaryButton(tr("Receive"));
    QPushButton *historyNow = createSecondaryButton(tr("History"));
    actions->addWidget(sendNow);
    actions->addWidget(receiveNow);
    actions->addWidget(historyNow);
    actions->addStretch();
    summaryLayout->addLayout(actions, 8, 0);

    connect(sendNow, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(receiveNow, SIGNAL(clicked()), this, SLOT(showReceive()));
    connect(historyNow, SIGNAL(clicked()), this, SLOT(showHistory()));

    QHBoxLayout *cards = new QHBoxLayout;
    cards->setSpacing(14);
    cards->addWidget(createAccountCard(tr("Main Account"), tr("0.9877 SFR"), tr("Default wallet account")));
    cards->addWidget(createAccountCard(tr("Membership"), tr("Active"), tr("Eligible for future block selection")));
    cards->addWidget(createAccountCard(tr("Chain Sync"), tr("0.13%"), tr("Prototype node status")));

    layout->addWidget(summary);
    layout->addLayout(cards);
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

    m_sendToEdit = new QLineEdit;
    m_sendToEdit->setPlaceholderText(tr("Recipient address"));
    layout->addWidget(m_sendToEdit);

    m_sendAmountEdit = new QLineEdit;
    m_sendAmountEdit->setPlaceholderText(tr("Amount in SFR"));
    layout->addWidget(m_sendAmountEdit);

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
    connect(clear, SIGNAL(clicked()), m_sendMemoEdit, SLOT(clear()));

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

QWidget *MainWindow::createHistoryPage()
{
    QFrame *page = makePanel("Panel");
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(26, 24, 26, 24);
    layout->setSpacing(14);

    layout->addWidget(createSectionTitle(tr("History")));

    m_historyTable = new QTableWidget(0, 5);
    QStringList headers;
    headers << tr("Date") << tr("Type") << tr("Account") << tr("Amount") << tr("Status");
    m_historyTable->setHorizontalHeaderLabels(headers);
    m_historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->setWordWrap(true);
    layout->addWidget(m_historyTable, 1);

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

    QCheckBox *startNetwork = new QCheckBox(tr("Start networking when the wallet opens"));
    QCheckBox *showAdvanced = new QCheckBox(tr("Show advanced chain diagnostics"));
    QCheckBox *confirmBeforeSend = new QCheckBox(tr("Require confirmation before sending"));
    confirmBeforeSend->setChecked(true);

    layout->addWidget(startNetwork);
    layout->addWidget(showAdvanced);
    layout->addWidget(confirmBeforeSend);
    layout->addStretch();
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

QLabel *MainWindow::createSectionTitle(const QString &text)
{
    return makeLabel(text, "SectionTitle");
}

void MainWindow::appendHistory(const QString &date, const QString &type, const QString &account, const QString &amount, const QString &status)
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
        if (column == 3 && amount.startsWith("+")) {
            item->setForeground(QColor("#18735d"));
        } else if (column == 3 && amount.startsWith("-")) {
            item->setForeground(QColor("#b3261e"));
        }
        m_historyTable->setItem(row, column, item);
    }
    m_historyTable->resizeRowsToContents();
}

void MainWindow::setActiveNav(QPushButton *activeButton)
{
    QList<QPushButton *> buttons;
    buttons << m_balanceButton << m_sendButton << m_receiveButton << m_historyButton << m_terminalButton << m_optionsButton;
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

QString MainWindow::receiveAddress() const
{
    return QString("03D245A704904A61B82E7876D123D5C561B990A391E427FC9019229439D2986680");
}

QString MainWindow::coreBinaryPath() const
{
    QStringList candidates;
    candidates << QDir::current().absoluteFilePath("bin/Safire");

    QDir appDir(QCoreApplication::applicationDirPath());
    candidates << appDir.absoluteFilePath("../../../../bin/Safire");

    for (int i = 0; i < candidates.size(); ++i) {
        QFileInfo file(candidates.at(i));
        if (file.exists() && file.isExecutable() && file.isFile()) {
            return file.canonicalFilePath();
        }
    }
    return QString();
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

    QFileInfo coreInfo(corePath);
    m_terminalProcess->setWorkingDirectory(coreInfo.absoluteDir().absolutePath() + "/..");
    QStringList args;
    args << "--api-port" << QString::number(m_backendPort);
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
    QString joined = object.value("joined").toString();
    QString sync = object.value("network_up_to_date").toString();
    QString latestBlock = object.value("latest_block_id").toString();
    QString peerCount = object.value("local_peers").toString();
    QString supply = object.value("currency_supply").toString();
    QString heartbeat = object.value("active_heartbeat").toString();
    bool progressOk = false;
    double syncProgress = object.value("sync_progress").toString().toDouble(&progressOk);
    int progressValue = progressOk ? static_cast<int>(syncProgress + 0.5) : 0;
    if (progressValue < 0) {
        progressValue = 0;
    }
    if (progressValue > 100) {
        progressValue = 100;
    }

    if (m_balanceLabel) {
        m_balanceLabel->setText(tr("%1 SFR").arg(balance));
    }
    if (m_networkLabel) {
        m_networkLabel->setText(tr("Joined: %1  Heartbeat: %2").arg(joined).arg(heartbeat));
    }
    if (m_syncLabel) {
        if (progressOk) {
            m_syncLabel->setText(tr("Sync: %1%  Latest block: %2").arg(progressValue).arg(latestBlock));
        } else {
            m_syncLabel->setText(tr("Sync: %1  Latest block: %2").arg(sync).arg(latestBlock));
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
    if (m_peerLabel) {
        m_peerLabel->setText(tr("Peers: %1").arg(peerCount));
    }
    if (m_supplyLabel) {
        m_supplyLabel->setText(tr("Supply: %1 SFR").arg(supply));
    }

    QString publicKey = object.value("public_key").toString();
    if (m_receiveAddressLabel && !publicKey.isEmpty()) {
        m_receiveAddressLabel->setText(publicKey);
    }
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
        appendHistory(tr("-"), tr("Error"), tr("Wallet history"), tr("-"), tr("Invalid response"));
        return;
    }

    QJsonObject object = document.object();
    if (object.value("status").toString() != "ok") {
        m_historyTable->setRowCount(0);
        appendHistory(tr("-"), tr("Status"), tr("Wallet history"), tr("-"), object.value("status").toString());
        return;
    }

    QJsonArray records = object.value("records").toArray();
    m_historyTable->setRowCount(0);
    if (records.isEmpty()) {
        appendHistory(tr("-"), tr("History"), tr("Wallet"), tr("-"), tr("No records"));
        return;
    }

    for (int i = 0; i < records.size(); ++i) {
        QJsonObject record = records.at(i).toObject();

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

        bool netOk = false;
        double netAmount = record.value("net").toString().toDouble(&netOk);
        QString amount = netOk ? formatSfrAmount(netAmount) : record.value("net").toString();

        QString block = record.value("block").toString();
        QString index = record.value("index").toString();
        QString status = tr("Block %1 #%2").arg(block).arg(index);

        appendHistory(date, direction, account, amount, status);
    }
}

void MainWindow::signIn()
{
    if (m_userEdit->text().trimmed().isEmpty() || m_passwordEdit->text().isEmpty()) {
        m_loginMessage->setText(tr("Enter a user and password."));
        return;
    }

    m_loginMessage->clear();
    m_userLabel->setText(tr("Signed in as %1").arg(m_userEdit->text().trimmed()));
    m_passwordEdit->clear();
    m_rootStack->setCurrentIndex(1);
    m_backendStartBlocked = false;
    ensureBackendRunning();
    refreshWalletStatus();
    m_statusTimer->start();
}

void MainWindow::signOut()
{
    m_statusTimer->stop();
    stopTerminal();
    m_rootStack->setCurrentIndex(0);
    m_passwordEdit->setFocus();
}

void MainWindow::showBalance()
{
    m_contentStack->setCurrentIndex(0);
    setActiveNav(m_balanceButton);
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

void MainWindow::showHistory()
{
    m_contentStack->setCurrentIndex(3);
    setActiveNav(m_historyButton);
    refreshWalletStatus();
}

void MainWindow::showTerminal()
{
    m_contentStack->setCurrentIndex(4);
    setActiveNav(m_terminalButton);
}

void MainWindow::showOptions()
{
    m_contentStack->setCurrentIndex(5);
    setActiveNav(m_optionsButton);
}

void MainWindow::copyReceiveAddress()
{
    QApplication::clipboard()->setText(receiveAddress());
    QMessageBox::information(this, tr("Safire"), tr("Address copied."));
}

void MainWindow::submitPayment()
{
    QString recipient = m_sendToEdit->text().trimmed();
    QString amount = m_sendAmountEdit->text().trimmed();
    if (recipient.isEmpty() || amount.isEmpty()) {
        QMessageBox::warning(this, tr("Safire"), tr("Enter a recipient address and amount."));
        return;
    }

    appendHistory(tr("Draft"), tr("Send"), tr("Main Account"), tr("-%1 SFR").arg(amount), tr("Needs review"));
    QMessageBox::information(this, tr("Safire"), tr("Payment draft added to history."));
    m_sendToEdit->clear();
    m_sendAmountEdit->clear();
    m_sendMemoEdit->clear();
    showHistory();
}

void MainWindow::startTerminal()
{
    m_backendStartBlocked = false;
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
    appendTerminalText(QString::fromLocal8Bit(bytes));
}

void MainWindow::terminalFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_terminalProcess) {
        QByteArray bytes = m_terminalProcess->readAllStandardOutput();
        appendTerminalText(QString::fromLocal8Bit(bytes));
    }
    appendTerminalText(tr("\nConsole backend stopped with exit code %1.\n").arg(exitCode));
    if (exitCode != 0) {
        m_backendStartBlocked = true;
    }
    if (m_terminalStatusLabel) {
        m_terminalStatusLabel->setText(tr("Stopped"));
    }
    if (m_terminalStartButton) {
        m_terminalStartButton->setEnabled(true);
    }
    if (m_terminalStopButton) {
        m_terminalStopButton->setEnabled(false);
    }
    if (m_networkLabel) {
        m_networkLabel->setText(tr("Backend: stopped"));
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
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 0);
        }
        return;
    }

    QUrl url(QString("http://127.0.0.1:%1/api/wallet/status").arg(m_backendPort));
    QNetworkRequest request(url);
    m_networkManager->get(request);

    QUrl historyUrl(QString("http://127.0.0.1:%1/api/wallet/history").arg(m_backendPort));
    QNetworkRequest historyRequest(historyUrl);
    m_networkManager->get(historyRequest);
}

void MainWindow::handleWalletStatusReply(QNetworkReply *reply)
{
    if (!reply) {
        return;
    }

    QByteArray body = reply->readAll();
    if (reply->error() != QNetworkReply::NoError) {
        if (m_syncLabel) {
            m_syncLabel->setText(tr("Sync: waiting for backend API"));
        }
        if (m_syncProgressBar) {
            m_syncProgressBar->setRange(0, 0);
        }
        reply->deleteLater();
        return;
    }

    QString path = reply->url().path();
    if (path == "/api/wallet/history") {
        applyWalletHistory(QString::fromUtf8(body));
    } else {
        applyWalletStatus(QString::fromUtf8(body));
    }
    reply->deleteLater();
}
