#include "ipc_common.h"
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

static int shm_fd = -1;
static ShmState *g_shm = nullptr;

static sem_t* g_sem_stage[NUM_STATIONS];
static sem_t* g_sem_ack[NUM_STATIONS];

static const char* sem_stage_name_fmt = "/sim_sem_stage_%d_if4001_v1";
static const char* sem_ack_name_fmt   = "/sim_sem_ack_%d_if4001_v1";

static std::string sem_name_buf(int idx, const char* fmt) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), fmt, idx);
    return std::string(tmp);
}

bool create_ipc() {
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) { perror("shm_open create"); return false; }
    if (ftruncate(shm_fd, sizeof(ShmState)) == -1) { perror("ftruncate"); return false; }
    void *m = mmap(NULL, sizeof(ShmState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (m == MAP_FAILED) { perror("mmap create"); return false; }
    g_shm = (ShmState*)m;
    g_shm->running = 0;
    for (int i=0;i<NUM_STATIONS;i++){ g_shm->station_done[i]=0; g_shm->station_paused[i]=0; }

    // create semaphores
    for (int i=0;i<NUM_STATIONS;i++) {
        std::string n = sem_name_buf(i, sem_stage_name_fmt);
        sem_unlink(n.c_str());
        g_sem_stage[i] = sem_open(n.c_str(), O_CREAT | O_EXCL, 0666, 0);
        if (g_sem_stage[i] == SEM_FAILED) { perror("sem_open stage create"); return false; }
        std::string a = sem_name_buf(i, sem_ack_name_fmt);
        sem_unlink(a.c_str());
        g_sem_ack[i] = sem_open(a.c_str(), O_CREAT | O_EXCL, 0666, 0);
        if (g_sem_ack[i] == SEM_FAILED) { perror("sem_open ack create"); return false; }
    }

    return true;
}

bool open_ipc() {
    if (shm_fd == -1) {
        shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
        if (shm_fd == -1) { perror("shm_open open"); return false; }
    }
    if (!g_shm) {
        void *m = mmap(NULL, sizeof(ShmState), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (m == MAP_FAILED) { perror("mmap open"); return false; }
        g_shm = (ShmState*)m;
    }

    for (int i=0;i<NUM_STATIONS;i++) {
        if (!g_sem_stage[i]) {
            std::string n = sem_name_buf(i, sem_stage_name_fmt);
            g_sem_stage[i] = sem_open(n.c_str(), 0);
            if (g_sem_stage[i] == SEM_FAILED) { perror("sem_open stage open"); return false; }
        }
        if (!g_sem_ack[i]) {
            std::string a = sem_name_buf(i, sem_ack_name_fmt);
            g_sem_ack[i] = sem_open(a.c_str(), 0);
            if (g_sem_ack[i] == SEM_FAILED) { perror("sem_open ack open"); return false; }
        }
    }

    return true;
}

void close_ipc() {
    if (g_shm) { munmap(g_shm, sizeof(ShmState)); g_shm = nullptr; }
    if (shm_fd != -1) { close(shm_fd); shm_fd = -1; }
    for (int i=0;i<NUM_STATIONS;i++) {
        if (g_sem_stage[i]) { sem_close(g_sem_stage[i]); g_sem_stage[i]=nullptr; }
        if (g_sem_ack[i]) { sem_close(g_sem_ack[i]); g_sem_ack[i]=nullptr; }
    }
}

void destroy_ipc() {
    close_ipc();
    shm_unlink(SHM_NAME);
    for (int i=0;i<NUM_STATIONS;i++) {
        std::string n = sem_name_buf(i, sem_stage_name_fmt);
        sem_unlink(n.c_str());
        std::string a = sem_name_buf(i, sem_ack_name_fmt);
        sem_unlink(a.c_str());
    }
}

sem_t* open_sem_stage(int idx) {
    if (idx<0 || idx>=NUM_STATIONS) return nullptr;
    return g_sem_stage[idx];
}
sem_t* open_sem_ack(int idx) {
    if (idx<0 || idx>=NUM_STATIONS) return nullptr;
    return g_sem_ack[idx];
}
