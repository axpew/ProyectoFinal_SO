#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <QObject>
#include <QThread>
#include <QAtomicInt>
#include <QTimer>

// Hilo 1: GeneralCleanThreads - Limpieza periódica
class CleanThread : public QThread
{
    Q_OBJECT
public:
    explicit CleanThread(QObject *parent = nullptr);
    void stop();

signals:
    void logMessage(const QString &msg);

protected:
    void run() override;

private:
    QAtomicInt running;
};

// Hilo 2: GeneralLogs - Recopilación de información
class LogsThread : public QThread
{
    Q_OBJECT
public:
    explicit LogsThread(QObject *parent = nullptr);
    void stop();

signals:
    void logMessage(const QString &msg);

protected:
    void run() override;

private:
    QAtomicInt running;
};

// Hilo 3: GeneralStats - Estadísticas del sistema
class StatsThread : public QThread
{
    Q_OBJECT
public:
    explicit StatsThread(QObject *parent = nullptr);
    void stop();

signals:
    void logMessage(const QString &msg);
    void statsUpdated(int productsProcessed, int threadsActive, int resourcesUsed);

protected:
    void run() override;

private:
    QAtomicInt running;
};

// Manager principal
class ThreadManager : public QObject
{
    Q_OBJECT
public:
    explicit ThreadManager(QObject *parent = nullptr);
    ~ThreadManager();

    void startAll();
    void stopAll();

signals:
    void log(const QString &msg);
    void statsUpdated(int productsProcessed, int threadsActive, int resourcesUsed);

public slots:
    void postLog(const QString &msg);

private:
    CleanThread *cleanThread;
    LogsThread *logsThread;
    StatsThread *statsThread;
    QAtomicInt running;
};

#endif // THREADMANAGER_H
