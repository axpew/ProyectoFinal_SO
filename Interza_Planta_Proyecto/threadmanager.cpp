#include "threadmanager.h"
#include "ipc_common.h"
#include <QDebug>
#include <QThread>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <QDateTime>

// ============================================================================
// CleanThread - Limpieza periódica del sistema
// ============================================================================
CleanThread::CleanThread(QObject *parent) : QThread(parent)
{
    running.storeRelease(1);
}

void CleanThread::stop()
{
    running.storeRelease(0);
}

void CleanThread::run()
{
    emit logMessage("🧹 GeneralCleanThreads: INICIADO - Monitoreo de limpieza activo");

    int cycle = 0;
    while (running.loadAcquire()) {
        // Esperar 60 segundos entre limpiezas
        for (int i = 0; i < 60 && running.loadAcquire(); ++i) {
            QThread::msleep(1000);
        }

        if (!running.loadAcquire()) break;

        cycle++;
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

        emit logMessage(QString("🧹 GeneralCleanThreads [%1]: Ejecutando limpieza automática #%2")
                            .arg(timestamp).arg(cycle));

        // Simular limpieza de recursos temporales
        emit logMessage(QString("   → Verificando recursos de memoria compartida..."));

        int fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd != -1) {
            ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ, MAP_SHARED, fd, 0);
            if (s != MAP_FAILED) {
                int activeStations = 0;
                for (int i = 0; i < NUM_STATIONS; i++) {
                    if (s->product_in_station[i].productId > 0) activeStations++;
                }
                emit logMessage(QString("   → Estado: %1 estaciones con productos activos")
                                    .arg(activeStations));
                munmap(s, sizeof(ShmState));
            }
            ::close(fd);
        }

        emit logMessage(QString("   ✓ Limpieza #%1 completada - Sistema optimizado").arg(cycle));
    }

    emit logMessage("🧹 GeneralCleanThreads: DETENIDO");
}

// ============================================================================
// LogsThread - Recopilación de información del sistema
// ============================================================================
LogsThread::LogsThread(QObject *parent) : QThread(parent)
{
    running.storeRelease(1);
}

void LogsThread::stop()
{
    running.storeRelease(0);
}

void LogsThread::run()
{
    emit logMessage("📋 GeneralLogs: INICIADO - Recopilación de información activa");

    int reportCount = 0;
    while (running.loadAcquire()) {
        // Reportar cada 45 segundos
        for (int i = 0; i < 45 && running.loadAcquire(); ++i) {
            QThread::msleep(1000);
        }

        if (!running.loadAcquire()) break;

        reportCount++;
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

        emit logMessage(QString("📋 GeneralLogs [%1]: Generando reporte del sistema #%2")
                            .arg(timestamp).arg(reportCount));

        int fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd != -1) {
            ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ, MAP_SHARED, fd, 0);
            if (s != MAP_FAILED) {
                emit logMessage(QString("   → Sistema: %1").arg(s->running ? "ACTIVO" : "DETENIDO"));
                emit logMessage(QString("   → Próximo ID de producto: %1").arg(s->next_product_id));

                int paused = 0;
                for (int i = 0; i < NUM_STATIONS; i++) {
                    if (s->station_paused[i]) paused++;
                }
                emit logMessage(QString("   → Estaciones pausadas: %1/%2").arg(paused).arg(NUM_STATIONS));

                munmap(s, sizeof(ShmState));
            }
            ::close(fd);
        }

        emit logMessage(QString("   ✓ Reporte #%1 completado").arg(reportCount));
    }

    emit logMessage("📋 GeneralLogs: DETENIDO");
}

// ============================================================================
// StatsThread - Estadísticas en tiempo real
// ============================================================================
StatsThread::StatsThread(QObject *parent) : QThread(parent)
{
    running.storeRelease(1);
}

void StatsThread::stop()
{
    running.storeRelease(0);
}

void StatsThread::run()
{
    emit logMessage("📊 GeneralStats: INICIADO - Monitoreo de estadísticas activo");

    int updateCount = 0;
    while (running.loadAcquire()) {
        // Actualizar cada 30 segundos
        for (int i = 0; i < 30 && running.loadAcquire(); ++i) {
            QThread::msleep(1000);
        }

        if (!running.loadAcquire()) break;

        updateCount++;
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");

        emit logMessage(QString("📊 GeneralStats [%1]: Actualizando estadísticas #%2")
                            .arg(timestamp).arg(updateCount));

        int fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (fd != -1) {
            ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ, MAP_SHARED, fd, 0);
            if (s != MAP_FAILED) {
                int productsInProgress = 0;
                int activeStations = 0;

                for (int i = 0; i < NUM_STATIONS; i++) {
                    if (s->product_in_station[i].productId > 0) {
                        productsInProgress++;
                        activeStations++;
                    }
                }

                int resourcesUsed = activeStations + (s->running ? 1 : 0);

                emit statsUpdated(s->next_product_id - 1, activeStations, resourcesUsed);

                emit logMessage(QString("   → Productos en proceso: %1").arg(productsInProgress));
                emit logMessage(QString("   → Estaciones activas: %1/%2").arg(activeStations).arg(NUM_STATIONS));
                emit logMessage(QString("   → Recursos en uso: %1").arg(resourcesUsed));

                munmap(s, sizeof(ShmState));
            }
            ::close(fd);
        }

        emit logMessage(QString("   ✓ Estadísticas actualizadas"));
    }

    emit logMessage("📊 GeneralStats: DETENIDO");
}

// ============================================================================
// ThreadManager - Gestor principal
// ============================================================================
ThreadManager::ThreadManager(QObject *parent) : QObject(parent),
    cleanThread(nullptr), logsThread(nullptr), statsThread(nullptr)
{
    running.storeRelease(0);
}

ThreadManager::~ThreadManager()
{
    stopAll();
}

void ThreadManager::postLog(const QString &msg)
{
    emit log(msg);
}

void ThreadManager::startAll()
{
    if (running.loadAcquire()) return;
    running.storeRelease(1);

    emit log("═══════════════════════════════════════════════════════");
    emit log("🚀 INICIANDO HILOS DE MANTENIMIENTO DEL SISTEMA");
    emit log("═══════════════════════════════════════════════════════");

    // Iniciar hilo de limpieza
    cleanThread = new CleanThread(this);
    connect(cleanThread, &CleanThread::logMessage, this, &ThreadManager::log);
    cleanThread->start();

    // Iniciar hilo de logs
    logsThread = new LogsThread(this);
    connect(logsThread, &LogsThread::logMessage, this, &ThreadManager::log);
    logsThread->start();

    // Iniciar hilo de estadísticas
    statsThread = new StatsThread(this);
    connect(statsThread, &StatsThread::logMessage, this, &ThreadManager::log);
    connect(statsThread, &StatsThread::statsUpdated, this, &ThreadManager::statsUpdated);
    statsThread->start();

    emit log("✅ Todos los hilos de mantenimiento iniciados correctamente");
    emit log("═══════════════════════════════════════════════════════");
}

void ThreadManager::stopAll()
{
    if (!running.loadAcquire()) return;
    running.storeRelease(0);

    emit log("═══════════════════════════════════════════════════════");
    emit log("🛑 DETENIENDO HILOS DE MANTENIMIENTO");

    if (cleanThread) {
        cleanThread->stop();
        cleanThread->wait();
        delete cleanThread;
        cleanThread = nullptr;
    }

    if (logsThread) {
        logsThread->stop();
        logsThread->wait();
        delete logsThread;
        logsThread = nullptr;
    }

    if (statsThread) {
        statsThread->stop();
        statsThread->wait();
        delete statsThread;
        statsThread = nullptr;
    }

    emit log("✅ Todos los hilos de mantenimiento detenidos");
    emit log("═══════════════════════════════════════════════════════");
}
