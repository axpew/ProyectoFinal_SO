#include "ipc_common.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>

bool create_ipc() {
    shm_unlink(SHM_NAME);

    for (int i = 0; i < NUM_STATIONS; i++) {
        char nameStage[128];
        snprintf(nameStage, sizeof(nameStage), "/sim_sem_stage_%d", i);
        sem_unlink(nameStage);

        char nameAck[128];
        snprintf(nameAck, sizeof(nameAck), "/sim_sem_ack_%d", i);
        sem_unlink(nameAck);
    }

    // Limpiar semáforo de transición
    sem_unlink(SEM_TRANSITION);

    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) return false;

    if (ftruncate(fd, sizeof(ShmState)) == -1) {
        ::close(fd);
        return false;
    }

    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s == MAP_FAILED) return false;

    memset(s, 0, sizeof(ShmState));
    s->running = 1;
    s->next_product_id = 1;
    munmap(s, sizeof(ShmState));

    for (int i = 0; i < NUM_STATIONS; i++) {
        char nameStage[128];
        snprintf(nameStage, sizeof(nameStage), "/sim_sem_stage_%d", i);
        sem_t* semStage = sem_open(nameStage, O_CREAT, 0666, 0);
        if (semStage == SEM_FAILED) return false;
        sem_close(semStage);

        char nameAck[128];
        snprintf(nameAck, sizeof(nameAck), "/sim_sem_ack_%d", i);
        sem_t* semAck = sem_open(nameAck, O_CREAT, 0666, 0);
        if (semAck == SEM_FAILED) return false;
        sem_close(semAck);
    }

    // Crear semáforo de transición (inicializado a 1 = disponible)
    sem_t* semTrans = sem_open(SEM_TRANSITION, O_CREAT, 0666, 1);
    if (semTrans == SEM_FAILED) return false;
    sem_close(semTrans);

    return true;
}

bool open_ipc() {
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) return false;
    ::close(fd);
    return true;
}

void close_ipc() {
}

void destroy_ipc() {
    shm_unlink(SHM_NAME);
    for (int i = 0; i < NUM_STATIONS; i++) {
        char nameStage[128];
        snprintf(nameStage, sizeof(nameStage), "/sim_sem_stage_%d", i);
        sem_unlink(nameStage);

        char nameAck[128];
        snprintf(nameAck, sizeof(nameAck), "/sim_sem_ack_%d", i);
        sem_unlink(nameAck);
    }
    sem_unlink(SEM_TRANSITION);
}

sem_t* open_sem_stage(int idx) {
    char name[128];
    snprintf(name, sizeof(name), "/sim_sem_stage_%d", idx);
    return sem_open(name, 0);
}

sem_t* open_sem_ack(int idx) {
    char name[128];
    snprintf(name, sizeof(name), "/sim_sem_ack_%d", idx);
    return sem_open(name, 0);
}

sem_t* open_sem_transition() {
    return sem_open(SEM_TRANSITION, 0);
}
