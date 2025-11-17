#include "productioncontroller.h"
#include "ipc_common.h"
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <QDebug>
#include <fcntl.h>        // O_CREAT, O_RDWR
#include <sys/mman.h>     // shm_open, mmap, munmap, PROT_READ, PROT_WRITE, MAP_SHARED
#include <sys/stat.h>     // S_IRUSR, S_IWUSR, mode flags
#include <unistd.h>       // close()
#include <semaphore.h>    // sem_open, sem_post, sem_wait
#include <chrono>
#include <thread>

extern "C" void _child_entry(int idx, int seed); // station_child.cpp

ProductionController::ProductionController(QObject *parent) : QObject(parent), ipc_created(false) {}

ProductionController::~ProductionController() {
    stopAllLines();
    destroy_ipc();
}

bool ProductionController::initializeIPC() {
    if (!create_ipc()) { emit logMessage("ERROR create_ipc"); return false; }
    if (!open_ipc()) { emit logMessage("ERROR open_ipc"); return false; }

    // set running flag and clear flags
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) { emit logMessage("shm_open init fail"); return false; }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s==MAP_FAILED) { emit logMessage("mmap fail"); return false; }
    s->running = 1;
    for (int i=0;i<NUM_STATIONS;i++){ s->station_done[i]=0; s->station_paused[i]=0; }
    munmap(s, sizeof(ShmState));

    // post stage 0 so station 0 can start
    sem_t* sem0 = open_sem_stage(0);
    if (sem0) sem_post(sem0);

    ipc_created = true;
    emit logMessage("IPC initialized");
    return true;
}

bool ProductionController::startAllLines() {
    if (!ipc_created) { emit logMessage("IPC not created"); return false; }

    for (int i=0;i<NUM_STATIONS;i++) {
        pid_t pid = fork();
        if (pid < 0) {
            emit logMessage(QString("fork failed for station %1").arg(i));
            return false;
        } else if (pid == 0) {
            // child process
            _child_entry(i, (int)getpid());
            _exit(0);
        } else {
            pids.push_back(pid);
            emit logMessage(QString("Forked station %1 pid=%2").arg(i).arg(pid));
        }
    }
    return true;
}

void ProductionController::stopAllLines() {
    // set running=0 and wake semaphores
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd != -1) {
        ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
        if (s!=MAP_FAILED) {
            s->running = 0;
            // wake semaphores so children exit
            for (int i=0;i<NUM_STATIONS;i++){
                sem_t* st = open_sem_stage(i);
                if (st) sem_post(st);
                sem_t* ak = open_sem_ack(i);
                if (ak) sem_post(ak);
            }
            munmap(s, sizeof(ShmState));
        }
        ::close(fd);
    }

    // give them a moment to exit gracefully
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // ensure children are terminated
    for (pid_t pid : pids) {
        if (pid > 0) {
            // try graceful first
            kill(pid, SIGTERM);
        }
    }

    // wait for children
    for (pid_t pid : pids) {
        if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            emit logMessage(QString("Stopped pid %1").arg(pid));
        }
    }
    pids.clear();
}

void ProductionController::restartAllLines() {
    emit logMessage("Restarting all lines...");
    stopAllLines();
    // destroy and re-create IPC and children
    destroy_ipc();
    ipc_created = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    if (!initializeIPC()) {
        emit logMessage("Failed to initialize IPC on restart");
        return;
    }
    if (!startAllLines()) {
        emit logMessage("Failed to start all lines on restart");
    } else {
        emit logMessage("Restart complete.");
    }
}

void ProductionController::destroyIPC() {
    destroy_ipc();
    ipc_created = false;
    emit logMessage("Destroyed IPC");
}

void ProductionController::pauseStation(int idx) {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) return;
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (s!=MAP_FAILED) {
        s->station_paused[idx] = 1;
        munmap(s, sizeof(ShmState));
    }
    ::close(fd);
    emit logMessage(QString("Paused station %1").arg(idx));
}

void ProductionController::resumeStation(int idx) {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) return;
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (s!=MAP_FAILED) {
        s->station_paused[idx] = 0;
        munmap(s, sizeof(ShmState));
    }
    ::close(fd);
    emit logMessage(QString("Resumed station %1").arg(idx));
}
