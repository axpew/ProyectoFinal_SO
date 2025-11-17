#ifndef PRODUCTIONCONTROLLER_H
#define PRODUCTIONCONTROLLER_H

#include <QObject>
#include <vector>
#include <sys/types.h>

#include <QList>
#include <QPair>
class ProductionController : public QObject
{
    Q_OBJECT
public:
    explicit ProductionController(QObject *parent = nullptr);
    ~ProductionController();

    bool initializeIPC(); // Para arrancar desde cero
    bool initializeIPC(int nextProductIdToRestore, const QList<QPair<int, int>>& productsToRestore); // Para restaurar

    bool startAllLines();
    void stopAllLines();
    void restartAllLines(); // nuevo: detiene, destruye IPC, reinicia todo
    void destroyIPC();

    void pauseStation(int idx);
    void resumeStation(int idx);

    void pauseAllStations();

signals:
    void logMessage(const QString &msg);

private:
    std::vector<pid_t> pids;
     bool ipc_created;
};

#endif // PRODUCTIONCONTROLLER_H
