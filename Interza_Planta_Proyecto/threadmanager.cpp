#include "threadmanager.h"
#include <QDebug>

ThreadManager::ThreadManager(QObject *parent) : QObject(parent),
    logsThread(nullptr), cleanThread(nullptr), statsThread(nullptr)
{
    running.storeRelease(0);
}

ThreadManager::~ThreadManager() {
    stopAll();
}

void ThreadManager::postLog(const QString &msg) {
    emit log(msg);
}

void ThreadManager::startAll() {
    // versión mínima: no threads que generen logs automáticos
    if (running.loadAcquire()) return;
    running.storeRelease(1);
    // Si en el futuro quieres hilos de mantenimiento, implementarlos aquí con QThread
}

void ThreadManager::stopAll() {
    if (!running.loadAcquire()) return;
    running.storeRelease(0);
    // limpiar hilos si hay
}
