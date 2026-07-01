#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QJsonArray>
#include <QMainWindow>
#include <QProcess>
#include <QStringList>

class QLabel;
class QCheckBox;
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
class HistoryChartWidget;
class LoadingSpinnerWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void signIn();
    void skipSignIn();
    void signOut();
    void showBalance();
    void showSend();
    void showReceive();
    void showContacts();
    void showHistory();
    void showMempool();
    void showBlockchain();
    void showPeers();
    void showTerminal();
    void showOptions();
    void copyReceiveAddress();
    void submitPayment();
    void joinNetwork();
    void setWalletName();
    void previousHistoryPage();
    void nextHistoryPage();
    void previousBlockchainPage();
    void nextBlockchainPage();
    void filterNetworkUsers(const QString &text);
    void filterBlockchainBlocks(const QString &text);
    void updateHistoryChart();
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
    void saveOptions();
    void resetBlockchain();
    void addWalletAccount();
    void switchWalletAccount(int index);
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
    QWidget *createBlockchainPage();
    QWidget *createPeersPage();
    QWidget *createTerminalPage();
    QWidget *createOptionsPage();
    QWidget *createAccountCard(const QString &name, const QString &balance, const QString &detail);
    QPushButton *createNavButton(const QString &text);
    QPushButton *createPrimaryButton(const QString &text);
    QPushButton *createSecondaryButton(const QString &text);
    QPushButton *createDangerButton(const QString &text);
    QLabel *createSectionTitle(const QString &text);
    QWidget *createLoadingOverlay(LoadingSpinnerWidget **spinner, const QString &text);
    QWidget *wrapWithLoadingOverlay(QWidget *content, QWidget *overlay);
    void appendHistory(const QString &date, const QString &type, const QString &account, const QString &amount, const QString &status, const QString &recordKey = QString());
    void appendTerminalText(const QString &text);
    void renderHistoryPage();
    void renderBlockchainPage();
    void setLoadingState(const QString &path, bool loading);
    void setLoadingWidget(QWidget *row, LoadingSpinnerWidget *spinner, bool loading);
    void applyWalletStatus(const QString &json);
    void applyWalletHistory(const QString &json);
    void applyMempool(const QString &json);
    void applyBlockchain(const QString &json);
    void applyPeers(const QString &json);
    void applyNetworkUsers(const QString &json);
    void applySendPaymentResult(const QString &json, bool transportError);
    void applyJoinNetworkResult(const QString &json, bool transportError);
    void applySetNameResult(const QString &json, bool transportError);
    void applyBlockchainResetResult(const QString &json, bool transportError);
    void applyWalletAccounts(const QString &json);
    void applyWalletAccountCreateResult(const QString &json, bool transportError);
    QString syncEtaText(double syncProgress,
                        bool progressOk,
                        qint64 latestBlockNumber,
                        bool latestBlockOk,
                        qint64 peerLatestBlockNumber,
                        bool peerLatestBlockOk);
    void loadContacts();
    void saveContacts();
    void refreshContactDropdown();
    void appendContact(const QString &name, const QString &address);
    QString historyRecordKey(const QJsonObject &record) const;
    QString selectedHistoryRecordKey() const;
    void restoreHistorySelection(const QString &recordKey);
    QString blockchainBlockKey(const QJsonObject &block) const;
    QString selectedBlockchainBlockKey() const;
    void restoreBlockchainSelection(const QString &blockKey);
    QString selectedNetworkUserAddress() const;
    void restoreNetworkUserSelection(const QString &address);
    void setActiveNav(QPushButton *activeButton);
    void requestJson(const QString &path);
    QString receiveAddress() const;
    QString coreBinaryPath() const;
    QString configFilePath() const;
    QString passwordHashFilePath() const;
    QString passwordHashFor(const QString &password) const;
    bool passwordHashExists() const;
    QString readPasswordHash() const;
    bool savePasswordHash(const QString &hash) const;
    void openWalletShell(const QString &statusText);
    bool publicPeerModeEnabled() const;
    QString storageProfile() const;
    bool saveStorageProfile(const QString &profile) const;
    bool ensureBackendRunning();

    QStackedWidget *m_rootStack;
    QStackedWidget *m_contentStack;
    QProcess *m_terminalProcess;
    QTimer *m_statusTimer;
    QNetworkAccessManager *m_networkManager;
    QStringList m_pendingRequests;
    QStringList m_loadedDataPaths;
    int m_backendPort;
    bool m_backendStartBlocked;
    bool m_backendLockDetected;
    bool m_backendRestartPending;
    bool m_loadingAccounts;
    double m_transactionFee;
    QString m_publicKey;
    QString m_publicName;
    QLineEdit *m_passwordEdit;
    QLabel *m_loginMessage;
    QLabel *m_loginHintLabel;
    QPushButton *m_loginPrimaryButton;
    QPushButton *m_loginSkipButton;
    QLabel *m_userLabel;
    QLabel *m_walletTitleLabel;
    QComboBox *m_accountCombo;
    QPushButton *m_addAccountButton;
    QLabel *m_balanceLabel;
    QLabel *m_estimatedBalanceLabel;
    QLabel *m_pendingBalanceLabel;
    QLabel *m_networkLabel;
    QLabel *m_syncLabel;
    QProgressBar *m_syncProgressBar;
    QLabel *m_syncEtaLabel;
    QLabel *m_peerLabel;
    QLabel *m_natLabel;
    QLabel *m_membershipJoinedLabel;
    QLabel *m_membershipHeartbeatLabel;
    QLabel *m_currentCreatorLabel;
    QLabel *m_nextCreatorLabel;
    QLabel *m_supplyLabel;
    QLabel *m_ledgerBalanceTotalLabel;
    QLabel *m_supplyDifferenceLabel;
    QLabel *m_userCountLabel;
    QLabel *m_blockCountLabel;
    QTableWidget *m_historyTable;
    QWidget *m_historyLoadingRow;
    LoadingSpinnerWidget *m_historyLoadingSpinner;
    QComboBox *m_historyRangeCombo;
    HistoryChartWidget *m_historyChart;
    QJsonArray m_walletHistoryRecords;
    int m_historyPage;
    QLabel *m_historyPageLabel;
    QPushButton *m_historyPrevButton;
    QPushButton *m_historyNextButton;
    QTableWidget *m_mempoolTable;
    QWidget *m_mempoolLoadingRow;
    LoadingSpinnerWidget *m_mempoolLoadingSpinner;
    QLineEdit *m_blockchainSearchEdit;
    QTableWidget *m_blockchainTable;
    QWidget *m_blockchainLoadingRow;
    LoadingSpinnerWidget *m_blockchainLoadingSpinner;
    QJsonArray m_blockchainBlocks;
    int m_blockchainPage;
    QLabel *m_blockchainPageLabel;
    QPushButton *m_blockchainPrevButton;
    QPushButton *m_blockchainNextButton;
    PeerMapWidget *m_peerMap;
    QTableWidget *m_peersTable;
    QWidget *m_peersLoadingRow;
    LoadingSpinnerWidget *m_peersLoadingSpinner;
    QLineEdit *m_contactSearchEdit;
    QTableWidget *m_networkUsersTable;
    QWidget *m_networkUsersLoadingRow;
    LoadingSpinnerWidget *m_networkUsersLoadingSpinner;
    QTableWidget *m_contactsTable;
    QComboBox *m_sendContactCombo;
    QLineEdit *m_sendToEdit;
    QLineEdit *m_sendAmountEdit;
    QLineEdit *m_sendFeeEdit;
    QTextEdit *m_sendMemoEdit;
    QLabel *m_receiveAddressLabel;
    QPlainTextEdit *m_terminalOutput;
    QLineEdit *m_terminalInput;
    QLabel *m_terminalStatusLabel;
    QPushButton *m_terminalStartButton;
    QPushButton *m_terminalStopButton;
    QCheckBox *m_publicPeerCheckBox;
    QComboBox *m_storageProfileComboBox;
    QPushButton *m_mainSendButton;
    QPushButton *m_joinNetworkButton;
    QPushButton *m_balanceButton;
    QPushButton *m_sendButton;
    QPushButton *m_receiveButton;
    QPushButton *m_contactsButton;
    QPushButton *m_historyButton;
    QPushButton *m_mempoolButton;
    QPushButton *m_blockchainButton;
    QPushButton *m_peersButton;
    QPushButton *m_terminalButton;
    QPushButton *m_optionsButton;
    double m_lastSyncProgress;
    qint64 m_lastSyncLatestBlock;
    qint64 m_lastSyncSampleMs;
};

#endif // MAINWINDOW_H
