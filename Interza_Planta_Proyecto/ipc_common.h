#ifndef IPC_COMMON_H
#define IPC_COMMON_H

#include <semaphore.h>

#define NUM_STATIONS 5
#define SHM_NAME "/sim_shm_if4001_v1"

struct ProductInfo {
    int productId;
};

struct ShmState {
    int running; // 1 running, 0 stop
    int station_done[NUM_STATIONS]; // 0 = idle, 1 = done, 2 = being processed by GUI
    int station_paused[NUM_STATIONS]; // 0 = running, 1 = paused

    // Un buffer para saber qué producto está en cada estación
    ProductInfo product_in_station[NUM_STATIONS];
    int next_product_id; // Un contador global para nuevos IDs de producto
};

bool create_ipc();
bool open_ipc(); // open existing mapping + sems
void close_ipc();
void destroy_ipc();

sem_t* open_sem_stage(int idx);
sem_t* open_sem_ack(int idx);

#endif // IPC_COMMON_H
