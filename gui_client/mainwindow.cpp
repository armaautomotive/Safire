#include "mainwindow.h"
#include <QDesktopWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    createHorizontalGroupBox();
    createGridGroupBox();

    //QFrame* frame = new QFrame(this);

    QWidget * wdg = new QWidget(this);
    //QWidget * wdg = new QWidget(frame);
    
    QHBoxLayout *hlay = new QHBoxLayout(wdg);

    QVBoxLayout *leftPaneVerticalLayout = new QVBoxLayout(wdg);

    QVBoxLayout *balanceViewVerticalLayout = new QVBoxLayout(wdg);


    // Add horizontal
    //hlay->addWidget(vlay);


    QPushButton * temp = new QPushButton("TEMP");
    balanceViewVerticalLayout->addWidget(temp);



    wdg->setStyleSheet("background-color: #00BFFF");

    bigEditor = new QTextEdit;
    bigEditor->setPlainText(tr("This widget takes up all the remaining space "
                               "in the top-level layout."));

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok
                                     | QDialogButtonBox::Cancel);

    connect(buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(horizontalGroupBox);
    mainLayout->addWidget(buttonBox);

    //setLayout(mainLayout);
    setWindowTitle(tr("Safire"));


    m_balance_view = new QPushButton( "Balance" );
    //m_balance_view->setText(tr("something"));
    m_balance_view->resize( 100, 30 );
    m_balance_view->setMaximumWidth(200);
    //a.setMainWidget( &hello );
    //m_balance_view.show();
    leftPaneVerticalLayout->addWidget(m_balance_view);

    m_history_view = new QPushButton("History");
    m_history_view->setMaximumWidth(200);
    leftPaneVerticalLayout->addWidget(m_history_view);
    m_send_view = new QPushButton("Send Payment");
    m_send_view->setMaximumWidth(200);
    leftPaneVerticalLayout->addWidget(m_send_view);

    m_receive_view = new QPushButton("Receive Payment");
    m_receive_view->setMaximumWidth(200);
    leftPaneVerticalLayout->addWidget(m_receive_view);
    m_options_view = new QPushButton("Options");
    m_options_view->setMaximumWidth(200);
    leftPaneVerticalLayout->addWidget(m_options_view);
   

    //QDesktopWidget dw;
    //int x=dw.width()*0.7;
    //int y=dw.height()*0.7;
    //QRect rect(0, 0, 640, 500);
    //wdg->setGeometry(rect);
    //hlay->setGeometry(rect);


    hlay->addLayout(leftPaneVerticalLayout);
    hlay->addLayout(balanceViewVerticalLayout);

    //frame->setLayout(wdg);
    //wdg->setLayout(vlay);
    wdg->setLayout(hlay);
    
    setCentralWidget(wdg);

    resize( 600, 500);

    //adjustSize();

    // Connect button signal to appropriate slot
    connect(m_balance_view, SIGNAL (released()), this, SLOT (handleButton()));
}

MainWindow::~MainWindow()
{

}


void MainWindow::createHorizontalGroupBox()
{
    horizontalGroupBox = new QGroupBox(tr("Horizontal layout"));
    QHBoxLayout *layout = new QHBoxLayout;

    for (int i = 0; i < NumButtons; ++i) {
        buttons[i] = new QPushButton(tr("Button %1").arg(i + 1));
        layout->addWidget(buttons[i]);
    }
    horizontalGroupBox->setLayout(layout);
}


void MainWindow::createGridGroupBox()
{
    gridGroupBox = new QGroupBox(tr("Grid layout"));

    for (int i = 0; i < NumGridRows; ++i) {
        labels[i] = new QLabel(tr("Line %1:").arg(i + 1));
        lineEdits[i] = new QLineEdit;

        //layout->addWidget(labels[i], (i + 1), 0);
        //layout->addWidget(lineEdits[i], (i + 1), 1);

        //gridGroupBox->addWidget(labels[i], i + 1, 0);
    }

    smallEditor = new QTextEdit;
    smallEditor->setPlainText(tr("This widget takes up about two thirds of the "
                                 "grid layout."));
    //layout->addWidget(smallEditor, 0, 2, 4, 1);


    //layout->setColumnStretch(1, 10);
    //layout->setColumnStretch(2, 20);
    //gridGroupBox->setLayout(layout);
}


void MainWindow::handleButton()
{
    // change the text
    m_balance_view->setText("Example");
    // resize button
    //m_balance_view->resize(100,100);
}

