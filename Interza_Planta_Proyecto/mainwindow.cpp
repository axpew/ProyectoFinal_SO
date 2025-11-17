#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QMetaObject>

#include <unistd.h>       // close()
#include <fcntl.h>        // O_CREAT, O_RDWR
#include <sys/mman.h>     // mmap, munmap, MAP_SHARED
#include <sys/stat.h>     // S_IRUSR, S_IWUSR

#include "ipc_common.h"
#include <QHBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), processedCount(0) {
    resize(1100, 720);
    central = new QWidget(this);
    setCentralWidget(central);
    mainLayout = new QVBoxLayout(central);

    titleLabel = new QLabel("Simulador Planta - Lobby", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight:bold; font-size:18px;");
    mainLayout->addWidget(titleLabel);

    counterLabel = new QLabel("Productos procesados: 0", this);
    counterLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(counterLabel);

    viewStack = new QStackedWidget(this);

    // prepare images list once
    QStringList beltImages = {
        ":/images/ensamble.png",
        ":/images/calidad.png",
        ":/images/empaque.png",
        ":/images/envoltorio.png",
        ":/images/pListo.png"
    };

    for (int i=0;i<NUM_STATIONS;i++){
        QWidget *page = new QWidget(this);
        QVBoxLayout *v = new QVBoxLayout(page);
        QStringList stationNames = {
            "Ensamblaje",
            "Control de Calidad",
            "Empaquetado",
            "Envoltorio",
            "Carga a Transporte"
        };

        QLabel *lab = new QLabel(QString("Estación %1 – %2")
                                     .arg(i+1)
                                     .arg(stationNames[i]));

        lab->setAlignment(Qt::AlignCenter);
        v->addWidget(lab);

        TransportBeltWidget *belt = new TransportBeltWidget(this);
        // if index out of range use placeholder path
        QString img = (i < beltImages.size()) ? beltImages[i] : QString();
        belt->setupWithImage(img);
        belts.append(belt);
        v->addWidget(belt);

        // control individual: botón de prueba removed from here (we will have global delete lot)
        viewStack->addWidget(page);
    }

    mainLayout->addWidget(viewStack, 1);

    bottomPanel = new QWidget(this);
    bottomLayout = new QHBoxLayout(bottomPanel);

    for (int i=0;i<NUM_STATIONS;i++){
        QPushButton *b = new QPushButton(QString("Ver Estación %1").arg(i+1));
        b->setProperty("lineIndex", i);
        connect(b, &QPushButton::clicked, this, &MainWindow::onLineButtonClicked);
        bottomLayout->addWidget(b);
        lineButtons.append(b);
    }

    pauseButton = new QPushButton("Pausar Producción");
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    bottomLayout->addWidget(pauseButton);

    resumeButton = new QPushButton("Reanudar Producción");
    connect(resumeButton, &QPushButton::clicked, this, &MainWindow::onResumeClicked);
    bottomLayout->addWidget(resumeButton);

    deleteLotButton = new QPushButton("Eliminar Lote (Reset)");
    connect(deleteLotButton, &QPushButton::clicked, this, &MainWindow::onDeleteLotClicked);
    bottomLayout->addWidget(deleteLotButton);

    shutdownButton = new QPushButton("Apagar Sistema");
    connect(shutdownButton, &QPushButton::clicked, this, &MainWindow::onShutdownClicked);
    bottomLayout->addWidget(shutdownButton);

    mainLayout->addWidget(bottomPanel);

    // styling general
    central->setStyleSheet("background: #F5EEDC;"); // beige suave
    bottomPanel->setStyleSheet("background: #AED6F1; padding: 8px; border-radius:6px;"); // azul pastel

    logWidget = new QTextEdit(this);
    logWidget->setReadOnly(true);
    logWidget->setFixedHeight(160);
    mainLayout->addWidget(logWidget);

    controller = new ProductionController(this);
    connect(controller, &ProductionController::logMessage, this, &MainWindow::onLogMessage);

    threadManager = new ThreadManager(this);
    connect(threadManager, &ThreadManager::log, this, &MainWindow::onLogMessage);
    threadManager->startAll();

    if (!controller->initializeIPC()) {
        QMessageBox::critical(this, "Error", "No se pudo inicializar IPC. Asegure permisos y vuelva a intentar.");
        return;
    }

    if (!controller->startAllLines()) {
        QMessageBox::warning(this, "Warn", "No se pudieron arrancar todos los procesos hijos.");
    }

    connect(&pollTimer, &QTimer::timeout, this, &MainWindow::pollSharedMemory);
    pollTimer.start(150);
}

MainWindow::~MainWindow() {
    pollTimer.stop();
    threadManager->stopAll();
    controller->stopAllLines();
    controller->destroyIPC();
}

void MainWindow::onLineButtonClicked() {
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    int idx = b->property("lineIndex").toInt();
    viewStack->setCurrentIndex(idx);
}

void MainWindow::onShutdownClicked() {
    controller->stopAllLines();
    controller->destroyIPC();
    QTimer::singleShot(500, qApp, &QCoreApplication::quit);
}

void MainWindow::pollSharedMemory() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        // shm not ready yet
        return;
    }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (s==MAP_FAILED) { ::close(fd); return; }

    // debug print the flags to help troubleshooting
    qDebug() << "pollSharedMemory: station_done ="
             << s->station_done[0] << s->station_done[1] << s->station_done[2]
             << s->station_done[3] << s->station_done[4];

    for (int i=0;i<NUM_STATIONS;i++){
        if (s->station_done[i] == 1) {
            s->station_done[i] = 2; // claim it
            int stationIndex = i;
            // start animation and supply finished callback (1 cycle)
            // if belt is paused (UI pause for station 0), we still allow current animation to run,
            // but pause prevents new productions from starting at station 0 (handled by child)
            belts[stationIndex]->startAnimation(1, [this, stationIndex]() {
                // when finished, ack the child
                sem_t* ack = open_sem_ack(stationIndex);
                if (ack) sem_post(ack);
                // update counter if last station
                if (stationIndex == NUM_STATIONS - 1) {
                    processedCount++;
                    counterLabel->setText(QString("Productos procesados: %1").arg(processedCount));
                    onLogMessage(QString("Producto finalizado. Total: %1").arg(processedCount));
                } else {
                    onLogMessage(QString("Estación %1: animación terminada, enviando ACK").arg(stationIndex+1));
                }
                // clear station_done
                int fd2 = shm_open(SHM_NAME, O_RDWR, 0666);
                if (fd2!=-1) {
                    ShmState* s2 = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd2, 0);
                    if (s2!=MAP_FAILED) {
                        s2->station_done[stationIndex] = 0;
                        munmap(s2, sizeof(ShmState));
                    }
                    ::close(fd2);
                }
            });

            onLogMessage(QString("Estación %1: GUI inició animación").arg(i+1));
        }
    }

    munmap(s, sizeof(ShmState));
    ::close(fd);
}

void MainWindow::onLogMessage(const QString &msg) {
    logWidget->append(msg);
}

// Pausa suave: deja terminar animaciones en curso, pero evita que estación 0 inicie nuevos procesos
void MainWindow::onPauseClicked() {
    controller->pauseStation(0); // pausa estación 1 (index 0)
    onLogMessage("UI: Pausa suave activada (estación 1 pausada)");
}

// Reanuda generación en estación 1
void MainWindow::onResumeClicked() {
    controller->resumeStation(0);
    onLogMessage("UI: Reanudado (estación 1)");
}

// Eliminar Lote: resetea contadores, logs, animaiones y reinicia procesos hijos (kill + restart)
void MainWindow::onDeleteLotClicked() {
    onLogMessage("UI: Ejecutando Eliminar Lote (reinicio completo) ...");

    // stop lines and destroy ipc
    controller->stopAllLines();
    controller->destroyIPC();

    // reset GUI state
    processedCount = 0;
    counterLabel->setText("Productos procesados: 0");
    logWidget->clear();

    // reset visual belts
    for (TransportBeltWidget* b : belts) {
        if (b) b->resetPosition();
    }

    // recreate IPC and start lines fresh
    if (!controller->initializeIPC()) {
        QMessageBox::critical(this, "Error", "No se pudo inicializar IPC después de reinicio.");
        return;
    }

    if (!controller->startAllLines()) {
        QMessageBox::warning(this, "Warn", "No se pudieron arrancar todos los procesos hijos después de reinicio.");
    } else {
        onLogMessage("UI: Reinicio completo, producción desde 0.");
    }
}
