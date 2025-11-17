#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QTimer>
#include <QVector>

#include "productioncontroller.h"
#include "threadmanager.h"
#include "transportbeltwidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onLineButtonClicked();
    void onPauseClicked();
    void onResumeClicked();
    void onShutdownClicked();
    void onDeleteLotClicked();     // NUEVO SLOT
    void pollSharedMemory();
    void onLogMessage(const QString &msg);

private:
    QWidget *central;
    QVBoxLayout *mainLayout;

    QLabel *titleLabel;
    QLabel *counterLabel;

    QStackedWidget *viewStack;

    QVector<TransportBeltWidget*> belts;
    QVector<QPushButton*> lineButtons;

    QWidget *bottomPanel;
    QHBoxLayout *bottomLayout;

    QPushButton *pauseButton;
    QPushButton *resumeButton;
    QPushButton *shutdownButton;
    QPushButton *deleteLotButton;  // NUEVO BOTÓN

    QTextEdit *logWidget;

    ProductionController *controller;
    ThreadManager *threadManager;

    QTimer pollTimer;

    int processedCount;
};

#endif // MAINWINDOW_H
