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

#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>

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
        ":/ensamble.png",
        ":/calidad.png",
        ":/empaque.png",
        ":/envoltorio.png",
        ":/pListo.png"
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

    //Cargamos el estado del archivo JSON.
    loadState();

    // nicializamos la IPC y arrancamos la producción
    //    Le pasamos los datos restaurados
    if (!controller->initializeIPC(m_nextProductIdToRestore, m_productsToRestore)) {
        QMessageBox::critical(this, "Error", "No se pudo inicializar la IPC y arrancar la producción.");
        return;
    }

    //Arrancamos los procesos hijo.
    if (!controller->startAllLines()) {
        QMessageBox::warning(this, "Warn", "No se pudieron arrancar todos los procesos hijos.");
    }


    connect(&pollTimer, &QTimer::timeout, this, &MainWindow::pollSharedMemory);
    pollTimer.start(150);
}

MainWindow::~MainWindow() {

    /*
    //Guardar el estado actual de la aplicación antes de cerrar
    saveState();

    pollTimer.stop();
    threadManager->stopAll();
    controller->stopAllLines();
    controller->destroyIPC();
*/
}


void MainWindow::onLineButtonClicked() {
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    int idx = b->property("lineIndex").toInt();
    viewStack->setCurrentIndex(idx);

}

void MainWindow::onShutdownClicked() {
     this->close(); // Llama a closeEvent() de forma segura
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

void MainWindow::saveState() {
    onLogMessage("--- Iniciando saveState ---");

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        onLogMessage("saveState: ERROR al abrir shm. No se puede guardar el estado en proceso.");
        // No retornamos, podemos guardar al menos el contador.
    }

    QJsonObject sessionInfoObject;
    QJsonArray inProgressArray;

    if (fd != -1) {
        ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ, MAP_SHARED, fd, 0);
        if (s != MAP_FAILED) {
            sessionInfoObject["nextProductId"] = s->next_product_id;
            for (int i = 0; i < NUM_STATIONS; ++i) {
                if (s->product_in_station[i].productId > 0) {
                    QJsonObject productObject;
                    productObject["productId"] = s->product_in_station[i].productId;
                    productObject["currentStation"] = i;
                    inProgressArray.append(productObject);
                }
            }
            munmap(s, sizeof(ShmState));
            onLogMessage(QString("saveState: Se encontraron %1 productos en proceso para guardar.").arg(inProgressArray.size()));
        } else {
            onLogMessage("saveState: ERROR al mapear shm. No se guardará el estado en proceso.");
        }
        ::close(fd);
    }

    sessionInfoObject["totalProductsFinished"] = processedCount;
    sessionInfoObject["lastClosed"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonObject finalStateObject;
    finalStateObject["sessionInfo"] = sessionInfoObject;
    finalStateObject["windowGeometry"] = QString(saveGeometry().toBase64());
    finalStateObject["inProgressProducts"] = inProgressArray;

    QJsonDocument doc(finalStateObject);


    //QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString path = QCoreApplication::applicationDirPath();
    QDir dir(path);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            onLogMessage("saveState: Creado directorio de datos: " + path);
        } else {
            onLogMessage("saveState: ERROR al crear directorio: " + path);
            return;
        }
    }
    QString filePath = path + "/app_state.json";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        onLogMessage("saveState: ERROR FATAL al abrir el archivo para escritura: " + filePath);
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    onLogMessage(QString("saveState: ¡Éxito! Estado guardado en %1").arg(filePath));
    onLogMessage("--- Finalizando saveState ---");
}

void MainWindow::loadState() {
    //Obtener la ruta del archivo de estado.
    //QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QString path = QCoreApplication::applicationDirPath();
    QString filePath = path + "/app_state.json";

    //Abrir y leer el archivo.
    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        onLogMessage("No se encontró archivo de estado previo. Iniciando desde cero.");
        return;
    }
    QByteArray savedData = file.readAll();
    file.close();

    //Parsear el documento JSON.
    QJsonDocument doc = QJsonDocument::fromJson(savedData);
    if (doc.isNull() || !doc.isObject()) {
        onLogMessage("Error: El archivo de estado está corrupto. Iniciando desde cero.");
        return;
    }
    QJsonObject finalStateObject = doc.object();

    //Restaurar la información de la sesión.
    if (finalStateObject.contains("sessionInfo") && finalStateObject["sessionInfo"].isObject()) {
        QJsonObject sessionInfoObject = finalStateObject["sessionInfo"].toObject();
        if (sessionInfoObject.contains("totalProductsFinished")) {
            processedCount = sessionInfoObject["totalProductsFinished"].toInt();
            counterLabel->setText(QString("Productos procesados: %1").arg(processedCount));
        }
        if (sessionInfoObject.contains("nextProductId")) {
            // Guardamos esto para usarlo al reiniciar la IPC.
            m_nextProductIdToRestore = sessionInfoObject["nextProductId"].toInt();
        }
    }

    //Restaurarla ventana.
    if (finalStateObject.contains("windowGeometry") && finalStateObject["windowGeometry"].isString()) {
        restoreGeometry(QByteArray::fromBase64(finalStateObject["windowGeometry"].toString().toUtf8()));
    }

    //Restaurar la lista de productos en proceso.
    if (finalStateObject.contains("inProgressProducts") && finalStateObject["inProgressProducts"].isArray()) {
        QJsonArray inProgressArray = finalStateObject["inProgressProducts"].toArray();
        m_productsToRestore.clear();
        for (const QJsonValue &val : inProgressArray) {
            QJsonObject productObject = val.toObject();
            if (productObject.contains("productId") && productObject.contains("currentStation")) {
                int prodId = productObject["productId"].toInt();
                int stationIdx = productObject["currentStation"].toInt();
                m_productsToRestore.append({prodId, stationIdx}); // Se añade a la lista
                onLogMessage(QString("Producto %1 para restaurar en estación %2").arg(prodId).arg(stationIdx));
            }
        }
    }

    onLogMessage("Estado de la aplicación cargado. Listo para restaurar la línea de producción.");
}

void MainWindow::closeEvent(QCloseEvent *event) {
    onLogMessage("Iniciando secuencia de apagado controlado...");

    //CONGELAR: Pausar todas las estaciones para que dejen de moverse.
    if (controller) {
        controller->pauseAllStations();
    }

    //ESPERAR: Para que los procesos hijo procesen la pausa.

    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    //Ahora que todo está quieto, guardamos el estado.
    saveState();

    //Detiene todo de forma definitiva.
    pollTimer.stop();
    if (threadManager) {
        threadManager->stopAll();
    }
    if (controller) {
        controller->stopAllLines();
        controller->destroyIPC();
    }

    onLogMessage("Apagado completado. Adiós.");
    event->accept();
}
