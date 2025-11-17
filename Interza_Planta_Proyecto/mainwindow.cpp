#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QMetaObject>

#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QJsonArray>
#include <QDateTime>

#include "ipc_common.h"
#include <QHBoxLayout>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), processedCount(0) {
    setMinimumSize(1000, 700);
    resize(1200, 800);

    central = new QWidget(this);
    setCentralWidget(central);
    mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ========== PANEL SUPERIOR ==========
    QWidget *topPanel = new QWidget(this);
    QVBoxLayout *topLayout = new QVBoxLayout(topPanel);
    topLayout->setSpacing(5);
    topLayout->setContentsMargins(10, 10, 10, 10);
    topPanel->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                            "stop:0 #2C3E50, stop:1 #34495E); "
                            "border-radius: 8px;");

    titleLabel = new QLabel("🏭 SIMULADOR DE LÍNEA DE PRODUCCIÓN", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight:bold; font-size:20px; color:#ECF0F1; padding:5px;");
    topLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel("Control de Producción Automatizada - IF-4001", this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("font-size:12px; color:#BDC3C7; padding:3px;");
    topLayout->addWidget(subtitleLabel);

    counterLabel = new QLabel("📦 Productos Completados: 0", this);
    counterLabel->setAlignment(Qt::AlignCenter);
    counterLabel->setStyleSheet("font-size:16px; color:#2ECC71; font-weight:bold; "
                                "background:#1E8449; padding:8px; border-radius:6px; margin:3px;");
    topLayout->addWidget(counterLabel);

    mainLayout->addWidget(topPanel);

    // ========== PANEL DE ESTADÍSTICAS ==========
    QWidget *statsPanel = new QWidget(this);
    QHBoxLayout *statsLayout = new QHBoxLayout(statsPanel);
    statsLayout->setContentsMargins(8, 5, 8, 5);
    statsPanel->setStyleSheet("background:#ECF0F1; border-radius:6px;");

    statsLabel = new QLabel("📊 Estaciones Activas: 0/5 | Recursos: 0 | Sistema: Iniciando...", this);
    statsLabel->setStyleSheet("font-size:12px; color:#2C3E50; font-weight:bold;");
    statsLayout->addWidget(statsLabel);

    mainLayout->addWidget(statsPanel);

    // ========== BARRA DE NOTIFICACIONES (NUEVO) ==========
    notificationLabel = new QLabel(this);
    notificationLabel->setAlignment(Qt::AlignCenter);
    notificationLabel->setFixedHeight(0);  // Inicialmente oculta
    notificationLabel->setStyleSheet("background:#3498DB; color:white; font-weight:bold; "
                                     "padding:10px; border-radius:6px; font-size:13px;");
    notificationLabel->hide();
    mainLayout->addWidget(notificationLabel);

    // ========== Resto del código sin cambios ==========
    viewStack = new QStackedWidget(this);

    QStringList beltImages = {
        ":/ensamble.png",
        ":/calidad.png",
        ":/empaque.png",
        ":/envoltorio.png",
        ":/pListo.png"
    };

    QStringList stationNames = {
        "⚙️ Ensamblaje",
        "✅ Control de Calidad",
        "📦 Empaquetado",
        "🎁 Envoltorio Final",
        "🚚 Carga a Transporte"
    };

    for (int i=0; i<NUM_STATIONS; i++){
        QWidget *page = new QWidget(this);
        page->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                            "stop:0 #FDFEFE, stop:1 #ECF0F1);");
        QVBoxLayout *v = new QVBoxLayout(page);
        v->setSpacing(5);
        v->setContentsMargins(5, 5, 5, 5);

        QLabel *lab = new QLabel(QString("ESTACIÓN %1: %2").arg(i+1).arg(stationNames[i]));
        lab->setAlignment(Qt::AlignCenter);
        lab->setStyleSheet("font-size:16px; font-weight:bold; "
                           "background:qlineargradient(x1:0, y1:0, x2:1, y2:0, "
                           "stop:0 #3498DB, stop:1 #2980B9); "
                           "padding:10px; border-radius:6px; color:white;");
        v->addWidget(lab);

        TransportBeltWidget *belt = new TransportBeltWidget(this);
        QString img = (i < beltImages.size()) ? beltImages[i] : QString();
        belt->setupWithImage(img);
        belts.append(belt);
        v->addWidget(belt);

        viewStack->addWidget(page);
    }

    mainLayout->addWidget(viewStack, 1);

    bottomPanel = new QWidget(this);
    bottomLayout = new QVBoxLayout(bottomPanel);
    bottomLayout->setSpacing(5);
    bottomLayout->setContentsMargins(10, 8, 10, 8);
    bottomPanel->setStyleSheet("background:#34495E; border-radius:8px;");

    QWidget *navRow = new QWidget();
    QHBoxLayout *navLayout = new QHBoxLayout(navRow);
    navLayout->setSpacing(5);
    navLayout->setContentsMargins(0, 0, 0, 0);

    for (int i=0; i<NUM_STATIONS; i++){
        QPushButton *b = new QPushButton(stationNames[i]);
        b->setProperty("lineIndex", i);
        b->setStyleSheet("QPushButton { background:#16A085; color:white; padding:8px; "
                         "border-radius:5px; font-weight:bold; font-size:11px; }"
                         "QPushButton:hover { background:#1ABC9C; }"
                         "QPushButton:pressed { background:#138D75; }");
        connect(b, &QPushButton::clicked, this, &MainWindow::onLineButtonClicked);
        navLayout->addWidget(b);
        lineButtons.append(b);
    }
    bottomLayout->addWidget(navRow);

    QWidget *controlRow = new QWidget();
    QHBoxLayout *controlLayout = new QHBoxLayout(controlRow);
    controlLayout->setSpacing(5);
    controlLayout->setContentsMargins(0, 0, 0, 0);

    pauseButton = new QPushButton("⏸️ Pausar");
    pauseButton->setStyleSheet("QPushButton { background:#F39C12; color:white; padding:8px; "
                               "border-radius:5px; font-weight:bold; }"
                               "QPushButton:hover { background:#F5B041; }");
    connect(pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseClicked);
    controlLayout->addWidget(pauseButton);

    resumeButton = new QPushButton("▶️ Reanudar");
    resumeButton->setStyleSheet("QPushButton { background:#27AE60; color:white; padding:8px; "
                                "border-radius:5px; font-weight:bold; }"
                                "QPushButton:hover { background:#2ECC71; }");
    connect(resumeButton, &QPushButton::clicked, this, &MainWindow::onResumeClicked);
    controlLayout->addWidget(resumeButton);

    deleteLotButton = new QPushButton("🔄 Reiniciar");
    deleteLotButton->setStyleSheet("QPushButton { background:#E74C3C; color:white; padding:8px; "
                                   "border-radius:5px; font-weight:bold; }"
                                   "QPushButton:hover { background:#EC7063; }");
    connect(deleteLotButton, &QPushButton::clicked, this, &MainWindow::onDeleteLotClicked);
    controlLayout->addWidget(deleteLotButton);

    shutdownButton = new QPushButton("⚠️ Apagar");
    shutdownButton->setStyleSheet("QPushButton { background:#95A5A6; color:white; padding:8px; "
                                  "border-radius:5px; font-weight:bold; }"
                                  "QPushButton:hover { background:#AAB7B8; }");
    connect(shutdownButton, &QPushButton::clicked, this, &MainWindow::onShutdownClicked);
    controlLayout->addWidget(shutdownButton);

    bottomLayout->addWidget(controlRow);
    mainLayout->addWidget(bottomPanel);

    QLabel *logTitle = new QLabel("📋 BITÁCORA DEL SISTEMA (GeneralLogs)", this);
    logTitle->setStyleSheet("font-weight:bold; font-size:13px; color:#2C3E50; "
                            "background:#D5DBDB; padding:6px; border-radius:5px;");
    mainLayout->addWidget(logTitle);

    logWidget = new QTextEdit(this);
    logWidget->setReadOnly(true);
    logWidget->setMinimumHeight(150);
    logWidget->setMaximumHeight(250);
    logWidget->setStyleSheet("background:#FDFEFE; border:2px solid #95A5A6; "
                             "border-radius:5px; font-family:monospace; font-size:10px; "
                             "padding:5px; color:#2C3E50;");
    mainLayout->addWidget(logWidget);

    central->setStyleSheet("background: qlineargradient(x1:0, y1:0, x2:0, y2:1, "
                           "stop:0 #ECF0F1, stop:1 #D5DBDB);");

    controller = new ProductionController(this);
    connect(controller, &ProductionController::logMessage, this, &MainWindow::onLogMessage);

    threadManager = new ThreadManager(this);
    connect(threadManager, &ThreadManager::log, this, &MainWindow::onLogMessage);
    connect(threadManager, &ThreadManager::statsUpdated, this, &MainWindow::onStatsUpdated);
    threadManager->startAll();

    loadState();

    if (!controller->initializeIPC(m_nextProductIdToRestore, m_productsToRestore)) {
        onLogMessage("❌ ERROR CRÍTICO: No se pudo inicializar IPC");
        return;
    }

    if (!controller->startAllLines()) {
        onLogMessage("⚠️ ADVERTENCIA: No se pudieron arrancar todos los procesos");
    }

    connect(&pollTimer, &QTimer::timeout, this, &MainWindow::pollSharedMemory);
    pollTimer.start(150);
}

MainWindow::~MainWindow() {
    // Destructor original - no cambiar
}

void MainWindow::onLineButtonClicked() {
    QPushButton *b = qobject_cast<QPushButton*>(sender());
    int idx = b->property("lineIndex").toInt();
    viewStack->setCurrentIndex(idx);
}

void MainWindow::onShutdownClicked() {
    this->close();
}

void MainWindow::pollSharedMemory() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        return;
    }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (s==MAP_FAILED) { ::close(fd); return; }

    // ACTUALIZAR ESTADÍSTICAS EN TIEMPO REAL
    int activeStations = 0;
    int productsInProgress = 0;
    for (int i = 0; i < NUM_STATIONS; i++) {
        if (s->product_in_station[i].productId > 0) {
            activeStations++;
            productsInProgress++;
        }
    }
    int resourcesUsed = activeStations + (s->running ? 1 : 0);

    // USAR next_product_id PARA TOTALES (productos creados, no completados)
    int totalProductsCreated = s->next_product_id - 1;

    // Actualizar la barra de estadísticas
    statsLabel->setText(QString("📊 Estaciones Activas: %1/5 | Recursos: %2 | Completados: %3 | Totales: %4 | En Proceso: %5")
                            .arg(activeStations)
                            .arg(resourcesUsed)
                            .arg(processedCount)        // Productos que YA salieron
                            .arg(totalProductsCreated)  // Productos que se han CREADO
                            .arg(totalProductsCreated - processedCount)); // Diferencia = en proceso

    for (int i=0; i<NUM_STATIONS; i++){
        if (s->station_done[i] == 1) {
            s->station_done[i] = 2;
            int stationIndex = i;

            belts[stationIndex]->startAnimation(1, [this, stationIndex]() {
                sem_t* ack = open_sem_ack(stationIndex);
                if (ack) sem_post(ack);

                if (stationIndex == NUM_STATIONS - 1) {
                    processedCount++;
                    counterLabel->setText(QString("📦 Productos Completados: %1").arg(processedCount));
                    onLogMessage(QString("✅ Producto finalizado. Total: %1").arg(processedCount));

                    // NOTIFICACIÓN cada 5 productos
                    if (processedCount % 5 == 0) {
                        showNotification(QString("¡%1 productos completados!").arg(processedCount), "success");
                    }
                } else {
                    onLogMessage(QString("➤ Estación %1: animación terminada, enviando ACK").arg(stationIndex+1));
                }

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

            onLogMessage(QString("🔄 Estación %1: GUI inició animación").arg(i+1));
        }
    }

    munmap(s, sizeof(ShmState));
    ::close(fd);
}

void MainWindow::onLogMessage(const QString &msg) {
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    logWidget->append(QString("[%1] %2").arg(timestamp).arg(msg));
}

void MainWindow::onStatsUpdated(int productsProcessed, int threadsActive, int resourcesUsed) {
    // Este método ahora solo es para logging interno de los hilos de mantenimiento
    // La GUI ya se actualiza en pollSharedMemory() en tiempo real
    // No hacemos nada aquí para evitar conflictos
}

void MainWindow::onPauseClicked() {
    controller->pauseStation(0);  // ← ESTO FALTABA
    onLogMessage("⏸️ UI: Pausa suave activada (estación 1 pausada)");
    showNotification("Producción pausada en Estación 1", "warning");
}

void MainWindow::onResumeClicked() {
    controller->resumeStation(0);
    onLogMessage("▶️ UI: Reanudado (estación 1)");
    showNotification("Producción reanudada", "success");
}

void MainWindow::onDeleteLotClicked() {
    onLogMessage("🔄 UI: Ejecutando Eliminar Lote (reinicio completo) ...");
    showNotification("Reiniciando sistema completo...", "warning");

    controller->stopAllLines();
    controller->destroyIPC();

    processedCount = 0;
    counterLabel->setText("📦 Productos Completados: 0");
    logWidget->clear();

    for (TransportBeltWidget* b : belts) {
        if (b) b->resetPosition();
    }

    if (!controller->initializeIPC()) {
        onLogMessage("❌ Error: No se pudo inicializar IPC después de reinicio.");
        showNotification("Error al inicializar IPC", "error");
        return;
    }

    if (!controller->startAllLines()) {
        onLogMessage("⚠️ Warn: No se pudieron arrancar todos los procesos hijos después de reinicio.");
        showNotification("Advertencia: algunos procesos fallaron", "warning");
    } else {
        onLogMessage("✅ UI: Reinicio completo, producción desde 0.");
        showNotification("Sistema reiniciado exitosamente", "success");
    }
}

void MainWindow::saveState() {
    onLogMessage("💾 --- Iniciando saveState ---");

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) {
        onLogMessage("⚠️ saveState: ERROR al abrir shm. No se puede guardar el estado en proceso.");
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
            onLogMessage(QString("📊 saveState: Se encontraron %1 productos en proceso para guardar.").arg(inProgressArray.size()));
        } else {
            onLogMessage("❌ saveState: ERROR al mapear shm. No se guardará el estado en proceso.");
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

    QString path = QCoreApplication::applicationDirPath();
    QDir dir(path);
    if (!dir.exists()) {
        if (dir.mkpath(".")) {
            onLogMessage("📁 saveState: Creado directorio de datos: " + path);
        } else {
            onLogMessage("❌ saveState: ERROR al crear directorio: " + path);
            return;
        }
    }
    QString filePath = path + "/app_state.json";

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        onLogMessage("❌ saveState: ERROR FATAL al abrir el archivo para escritura: " + filePath);
        return;
    }

    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    onLogMessage(QString("✅ saveState: ¡Éxito! Estado guardado en %1").arg(filePath));
    onLogMessage("💾 --- Finalizando saveState ---");
}

void MainWindow::loadState() {
    QString path = QCoreApplication::applicationDirPath();
    QString filePath = path + "/app_state.json";

    QFile file(filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        onLogMessage("ℹ️ No se encontró archivo de estado previo. Iniciando desde cero.");
        return;
    }
    QByteArray savedData = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(savedData);
    if (doc.isNull() || !doc.isObject()) {
        onLogMessage("⚠️ Error: El archivo de estado está corrupto. Iniciando desde cero.");
        return;
    }
    QJsonObject finalStateObject = doc.object();

    if (finalStateObject.contains("sessionInfo") && finalStateObject["sessionInfo"].isObject()) {
        QJsonObject sessionInfoObject = finalStateObject["sessionInfo"].toObject();
        if (sessionInfoObject.contains("totalProductsFinished")) {
            processedCount = sessionInfoObject["totalProductsFinished"].toInt();
            counterLabel->setText(QString("📦 Productos Completados: %1").arg(processedCount));
        }
        if (sessionInfoObject.contains("nextProductId")) {
            m_nextProductIdToRestore = sessionInfoObject["nextProductId"].toInt();
        }
    }

    if (finalStateObject.contains("windowGeometry") && finalStateObject["windowGeometry"].isString()) {
        restoreGeometry(QByteArray::fromBase64(finalStateObject["windowGeometry"].toString().toUtf8()));
    }

    if (finalStateObject.contains("inProgressProducts") && finalStateObject["inProgressProducts"].isArray()) {
        QJsonArray inProgressArray = finalStateObject["inProgressProducts"].toArray();
        m_productsToRestore.clear();
        for (const QJsonValue &val : inProgressArray) {
            QJsonObject productObject = val.toObject();
            if (productObject.contains("productId") && productObject.contains("currentStation")) {
                int prodId = productObject["productId"].toInt();
                int stationIdx = productObject["currentStation"].toInt();
                m_productsToRestore.append({prodId, stationIdx});
                onLogMessage(QString("📦 Producto %1 para restaurar en estación %2").arg(prodId).arg(stationIdx));
            }
        }
    }

    onLogMessage("✅ Estado de la aplicación cargado. Listo para restaurar la línea de producción.");
}

void MainWindow::showNotification(const QString &message, const QString &type) {
    // Configurar estilo según tipo
    QString bgColor, icon;
    if (type == "success") {
        bgColor = "#27AE60";  // Verde
        icon = "✅";
    } else if (type == "warning") {
        bgColor = "#F39C12";  // Naranja
        icon = "⚠️";
    } else if (type == "error") {
        bgColor = "#E74C3C";  // Rojo
        icon = "❌";
    } else {
        bgColor = "#3498DB";  // Azul (info)
        icon = "ℹ️";
    }

    notificationLabel->setText(icon + " " + message);
    notificationLabel->setStyleSheet(QString("background:%1; color:white; font-weight:bold; "
                                             "padding:10px; border-radius:6px; font-size:13px;")
                                         .arg(bgColor));

    // Animación de aparición suave
    notificationLabel->setFixedHeight(0);
    notificationLabel->show();

    QPropertyAnimation *showAnim = new QPropertyAnimation(notificationLabel, "maximumHeight");
    showAnim->setDuration(300);  // 300ms para aparecer
    showAnim->setStartValue(0);
    showAnim->setEndValue(50);
    showAnim->start(QAbstractAnimation::DeleteWhenStopped);

    // Auto-ocultar después de 4 segundos
    QTimer::singleShot(4000, this, [this]() {
        QPropertyAnimation *hideAnim = new QPropertyAnimation(notificationLabel, "maximumHeight");
        hideAnim->setDuration(300);
        hideAnim->setStartValue(50);
        hideAnim->setEndValue(0);
        connect(hideAnim, &QPropertyAnimation::finished, notificationLabel, &QLabel::hide);
        hideAnim->start(QAbstractAnimation::DeleteWhenStopped);
    });
}

void MainWindow::closeEvent(QCloseEvent *event) {
    onLogMessage("🔴 Iniciando secuencia de apagado controlado...");

    if (controller) {
        controller->pauseAllStations();
    }

    QEventLoop loop;
    QTimer::singleShot(100, &loop, &QEventLoop::quit);
    loop.exec();

    saveState();

    pollTimer.stop();
    if (threadManager) {
        threadManager->stopAll();
    }
    if (controller) {
        controller->stopAllLines();
        controller->destroyIPC();
    }

    onLogMessage("✅ Apagado completado. Adiós.");
    event->accept();
}
