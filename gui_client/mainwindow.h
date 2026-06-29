#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QStackedWidget;
class QTableWidget;
class QTextEdit;
class QWidget;
class QCloseEvent;

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
    void showHistory();
    void showTerminal();
    void showOptions();
    void copyReceiveAddress();
    void submitPayment();
    void startTerminal();
    void stopTerminal();
    void sendTerminalCommand();
    void readTerminalOutput();
    void terminalFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void terminalError(QProcess::ProcessError error);

private:
    void closeEvent(QCloseEvent *event);
    QWidget *createLoginPage();
    QWidget *createShellPage();
    QWidget *createBalancePage();
    QWidget *createSendPage();
    QWidget *createReceivePage();
    QWidget *createHistoryPage();
    QWidget *createTerminalPage();
    QWidget *createOptionsPage();
    QWidget *createAccountCard(const QString &name, const QString &balance, const QString &detail);
    QPushButton *createNavButton(const QString &text);
    QPushButton *createPrimaryButton(const QString &text);
    QPushButton *createSecondaryButton(const QString &text);
    QLabel *createSectionTitle(const QString &text);
    void appendHistory(const QString &date, const QString &type, const QString &account, const QString &amount, const QString &status);
    void appendTerminalText(const QString &text);
    void setActiveNav(QPushButton *activeButton);
    QString receiveAddress() const;
    QString coreBinaryPath() const;

    QStackedWidget *m_rootStack;
    QStackedWidget *m_contentStack;
    QProcess *m_terminalProcess;
    QLineEdit *m_userEdit;
    QLineEdit *m_passwordEdit;
    QLabel *m_loginMessage;
    QLabel *m_userLabel;
    QLabel *m_balanceLabel;
    QLabel *m_networkLabel;
    QTableWidget *m_historyTable;
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
    QPushButton *m_historyButton;
    QPushButton *m_terminalButton;
    QPushButton *m_optionsButton;
};

#endif // MAINWINDOW_H
