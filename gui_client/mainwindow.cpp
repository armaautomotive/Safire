#include "mainwindow.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QColor>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QList>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
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

}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_rootStack(new QStackedWidget(this)),
      m_contentStack(0),
      m_userEdit(0),
      m_passwordEdit(0),
      m_loginMessage(0),
      m_userLabel(0),
      m_balanceLabel(0),
      m_networkLabel(0),
      m_historyTable(0),
      m_sendToEdit(0),
      m_sendAmountEdit(0),
      m_sendMemoEdit(0),
      m_receiveAddressLabel(0),
      m_balanceButton(0),
      m_sendButton(0),
      m_receiveButton(0),
      m_historyButton(0),
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
        "QTableWidget { background: white; border: 1px solid #dce7ea; border-radius: 6px; gridline-color: #edf2f3; }"
        "QHeaderView::section { background: #edf4f5; color: #41545a; padding: 8px; border: 0; font-weight: 700; }"
    );

    m_rootStack->addWidget(createLoginPage());
    m_rootStack->addWidget(createShellPage());
    setCentralWidget(m_rootStack);
}

MainWindow::~MainWindow()
{
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
    m_optionsButton = createNavButton(tr("Options"));

    nav->addWidget(m_balanceButton);
    nav->addWidget(m_sendButton);
    nav->addWidget(m_receiveButton);
    nav->addWidget(m_historyButton);
    nav->addWidget(m_optionsButton);
    nav->addStretch();

    QPushButton *signOutButton = createSecondaryButton(tr("Lock Wallet"));
    nav->addWidget(signOutButton);

    m_contentStack = new QStackedWidget;
    m_contentStack->addWidget(createBalancePage());
    m_contentStack->addWidget(createSendPage());
    m_contentStack->addWidget(createReceivePage());
    m_contentStack->addWidget(createHistoryPage());
    m_contentStack->addWidget(createOptionsPage());

    connect(m_balanceButton, SIGNAL(clicked()), this, SLOT(showBalance()));
    connect(m_sendButton, SIGNAL(clicked()), this, SLOT(showSend()));
    connect(m_receiveButton, SIGNAL(clicked()), this, SLOT(showReceive()));
    connect(m_historyButton, SIGNAL(clicked()), this, SLOT(showHistory()));
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

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *sendNow = createPrimaryButton(tr("Send"));
    QPushButton *receiveNow = createSecondaryButton(tr("Receive"));
    QPushButton *historyNow = createSecondaryButton(tr("History"));
    actions->addWidget(sendNow);
    actions->addWidget(receiveNow);
    actions->addWidget(historyNow);
    actions->addStretch();
    summaryLayout->addLayout(actions, 4, 0);

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
    layout->addWidget(m_historyTable, 1);

    appendHistory(tr("2026-06-28"), tr("Receive"), tr("Main Account"), tr("+0.9877 SFR"), tr("Confirmed"));
    appendHistory(tr("2026-06-27"), tr("Membership"), tr("Main Account"), tr("Active"), tr("Local"));
    appendHistory(tr("2026-06-26"), tr("Sync"), tr("Node"), tr("0.13%"), tr("In progress"));

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
        }
        m_historyTable->setItem(row, column, item);
    }
}

void MainWindow::setActiveNav(QPushButton *activeButton)
{
    QList<QPushButton *> buttons;
    buttons << m_balanceButton << m_sendButton << m_receiveButton << m_historyButton << m_optionsButton;
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
}

void MainWindow::signOut()
{
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
}

void MainWindow::showOptions()
{
    m_contentStack->setCurrentIndex(4);
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
