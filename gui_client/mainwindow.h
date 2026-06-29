#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>

class QLabel;
class QComboBox;
class QLineEdit;
class QNetworkAccessManager;
class QNetworkReply;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QTimer;
class QWidget;
class QCloseEvent;
class PeerMapWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void signIn();
    void signOut();
    void showBalance();
    void showSend();
    void showReceive();
    void showContacts();
    void showHistory();
    void showMempool();
    void showPeers();
    void showTerminal();
    void showOptions();
    void copyReceiveAddress();
    void submitPayment();
    void setWalletName();
    void filterNetworkUsers(const QString &text);
    void addSelectedNetworkUserToContacts();
    void removeSelectedContact();
    void useSelectedContactForSend();
    void setSendRecipientFromContact(int index);
    void startTerminal();
    void stopTerminal();
    void sendTerminalCommand();
    void readTerminalOutput();
    void terminalFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void terminalError(QProcess::ProcessError error);
    void refreshWalletStatus();
    void handleWalletStatusReply(QNetworkReply *reply);

private:
    void closeEvent(QCloseEvent *event);
    QWidget *createLoginPage();
    QWidget *createShellPage();
    QWidget *createBalancePage();
    QWidget *createSendPage();
    QWidget *createReceivePage();
    QWidget *createContactsPage();
    QWidget *createHistoryPage();
    QWidget *createMempoolPage();
    QWidget *createPeersPage();
    QWidget *createTerminalPage();
    QWidget *createOptionsPage();
    QWidget *createAccountCard(const QString &name, const QString &balance, const QString &detail);
    QPushButton *createNavButton(const QString &text);
    QPushButton *createPrimaryButton(const QString &text);
    QPushButton *createSecondaryButton(const QString &text);
    QLabel *createSectionTitle(const QString &text);
    void appendHistory(const QString &date, const QString &type, const QString &account, const QString &amount, const QString &status);
    void appendTerminalText(const QString &text);
    void applyWalletStatus(const QString &json);
    void applyWalletHistory(const QString &json);
    void applyMempool(const QString &json);
    void applyPeers(const QString &json);
    void applyNetworkUsers(const QString &json);
    void applySendPaymentResult(const QString &json, bool transportError);
    void applySetNameResult(const QString &json, bool transportError);
    void loadContacts();
    void saveContacts();
    void refreshContactDropdown();
    void appendContact(const QString &name, const QString &address);
    void setActiveNav(QPushButton *activeButton);
    QString receiveAddress() const;
    QString coreBinaryPath() const;
    bool ensureBackendRunning();

    QStackedWidget *m_rootStack;
    QStackedWidget *m_contentStack;
    QProcess *m_terminalProcess;
    QTimer *m_statusTimer;
    QNetworkAccessManager *m_networkManager;
    int m_backendPort;
    bool m_backendStartBlocked;
    bool m_backendLockDetected;
    double m_transactionFee;
    QString m_publicKey;
    QString m_publicName;
    QLineEdit *m_userEdit;
    QLineEdit *m_passwordEdit;
    QLabel *m_loginMessage;
    QLabel *m_userLabel;
    QLabel *m_walletTitleLabel;
    QLabel *m_balanceLabel;
    QLabel *m_networkLabel;
    QLabel *m_syncLabel;
    QProgressBar *m_syncProgressBar;
    QLabel *m_peerLabel;
    QLabel *m_membershipJoinedLabel;
    QLabel *m_membershipHeartbeatLabel;
    QLabel *m_currentCreatorLabel;
    QLabel *m_nextCreatorLabel;
    QLabel *m_supplyLabel;
    QLabel *m_userCountLabel;
    QLabel *m_blockCountLabel;
    QTableWidget *m_historyTable;
    QTableWidget *m_mempoolTable;
    PeerMapWidget *m_peerMap;
    QTableWidget *m_peersTable;
    QLineEdit *m_contactSearchEdit;
    QTableWidget *m_networkUsersTable;
    QTableWidget *m_contactsTable;
    QComboBox *m_sendContactCombo;
    QLineEdit *m_sendToEdit;
    QLineEdit *m_sendAmountEdit;
    QTextEdit *m_sendMemoEdit;
    QLabel *m_receiveAddressLabel;
    QPlainTextEdit *m_terminalOutput;
    QLineEdit *m_terminalInput;
    QLabel *m_terminalStatusLabel;
    QPushButton *m_terminalStartButton;
    QPushButton *m_terminalStopButton;
    QPushButton *m_balanceButton;
    QPushButton *m_sendButton;
    QPushButton *m_receiveButton;
    QPushButton *m_contactsButton;
    QPushButton *m_historyButton;
    QPushButton *m_mempoolButton;
    QPushButton *m_peersButton;
    QPushButton *m_terminalButton;
    QPushButton *m_optionsButton;
};

#endif // MAINWINDOW_H
