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
#include <QCloseEvent>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QPropertyAnimation>  // NUEVO
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
    void onDeleteLotClicked();
    void pollSharedMemory();
    void onLogMessage(const QString &msg);
    void onStatsUpdated(int productsProcessed, int threadsActive, int resourcesUsed);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void showNotification(const QString &message, const QString &type = "info");

    QWidget *central;
    QVBoxLayout *mainLayout;

    QLabel *titleLabel;
    QLabel *counterLabel;
    QLabel *statsLabel;
    QLabel *notificationLabel;  //Barra de notificaciones

    QStackedWidget *viewStack;

    QVector<TransportBeltWidget*> belts;
    QVector<QPushButton*> lineButtons;

    QWidget *bottomPanel;
    QVBoxLayout *bottomLayout;

    QPushButton *pauseButton;
    QPushButton *resumeButton;
    QPushButton *shutdownButton;
    QPushButton *deleteLotButton;

    QTextEdit *logWidget;

    ProductionController *controller;
    ThreadManager *threadManager;

    QTimer pollTimer;

    int processedCount;

    void saveState();
    void loadState();
    QList<QPair<int, int>> m_productsToRestore;
    int m_nextProductIdToRestore = 1;
};

#endif // MAINWINDOW_H
