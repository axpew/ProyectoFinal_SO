#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <semaphore.h>

#define NUM_STATIONS 5
#define SHM_NAME "/sim_shm_if4001_v1"
#define SEM_TRANSITION "/sim_sem_transition"  // Nuevo

struct ProductInfo {
    int productId;
};

struct ShmState {
    int running;
    int station_done[NUM_STATIONS];
    int station_paused[NUM_STATIONS];
    ProductInfo product_in_station[NUM_STATIONS];
    int next_product_id;
};

bool create_ipc();
bool open_ipc();
void close_ipc();
void destroy_ipc();

sem_t* open_sem_stage(int idx);
sem_t* open_sem_ack(int idx);
sem_t* open_sem_transition();  // Nuevo

#endif // IPC_COMMON_H
