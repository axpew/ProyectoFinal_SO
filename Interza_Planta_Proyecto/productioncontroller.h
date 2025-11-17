#ifndef PRODUCTIONCONTROLLER_H
#define PRODUCTIONCONTROLLER_H

#include <QObject>
#include <vector>
#include <sys/types.h>

class ProductionController : public QObject
{
    Q_OBJECT
public:
    explicit ProductionController(QObject *parent = nullptr);
    ~ProductionController();

    bool initializeIPC();
    bool startAllLines();
    void stopAllLines();
    void restartAllLines(); // nuevo: detiene, destruye IPC, reinicia todo
    void destroyIPC();

    void pauseStation(int idx);
    void resumeStation(int idx);

signals:
    void logMessage(const QString &msg);

private:
    std::vector<pid_t> pids;
    bool ipc_created;
};

#endif // PRODUCTIONCONTROLLER_H
