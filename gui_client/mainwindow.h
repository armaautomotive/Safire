#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void handleButton();

private:
    void createHorizontalGroupBox();
    void createGridGroupBox();

    enum { NumGridRows = 3, NumButtons = 4 };

    QVBoxLayout *mainLayout = new QVBoxLayout;
    QGroupBox *horizontalGroupBox;
    QGroupBox *gridGroupBox;
    QTextEdit *bigEditor;
    QPushButton *buttons[NumButtons];
    QDialogButtonBox *buttonBox;
    QLabel *labels[NumGridRows];
    QLineEdit *lineEdits[NumGridRows];
    QTextEdit *smallEditor;

    QPushButton *m_balance_view;
    QPushButton *m_history_view;
    QPushButton *m_send_view;
    QPushButton *m_receive_view;
    QPushButton *m_options_view;



};

#endif // MAINWINDOW_H
