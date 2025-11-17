#ifndef THREADMANAGER_H
#define THREADMANAGER_H

#include <QObject>
#include <QThread>
#include <QAtomicInt>

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

public slots:
    void postLog(const QString &msg);

private:
    QThread *logsThread;
    QThread *cleanThread;
    QThread *statsThread;
    QAtomicInt running;
};

#endif // THREADMANAGER_H
