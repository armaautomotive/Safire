#include "mainwindow.h"
#include <QDesktopWidget>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    createHorizontalGroupBox();
    createGridGroupBox();

    //QFrame* frame = new QFrame(this);

    QWidget * wdg = new QWidget(this);
    //QWidget * wdg = new QWidget(frame);
    
    QVBoxLayout *vlay = new QVBoxLayout(wdg);

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


    hello = new QPushButton( "Hello world!" );
    hello->setText(tr("something"));
    hello->resize( 100, 30 );
    //a.setMainWidget( &hello );
    //hello.show();
    vlay->addWidget(hello);

    QPushButton *btn2 = new QPushButton("btn2");
    vlay->addWidget(btn2);
    QPushButton *btn3 = new QPushButton("btn3");
    vlay->addWidget(btn3);
   

    QDesktopWidget dw;
    int x=dw.width()*0.7;
    int y=dw.height()*0.7;
    QRect rect(0, 0, 240, 500);
    wdg->setGeometry(rect); 


    //frame->setLayout(wdg);

    wdg->setLayout(vlay); 
    
    setCentralWidget(wdg);

    resize( 600, 500);

    adjustSize();

    // Connect button signal to appropriate slot
    connect(hello, SIGNAL (released()), this, SLOT (handleButton()));
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
    hello->setText("Example");
    // resize button
    //hello->resize(100,100);
}

