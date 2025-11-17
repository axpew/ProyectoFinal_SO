#include "station_child.h"
#include "ipc_common.h"

#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstdio>
#include <signal.h>
#include <time.h>
#include <semaphore.h>

extern "C" void _child_entry(int idx, int seed) {
    if (!open_ipc()) {
        fprintf(stderr, "Child %d: cannot open ipc\n", idx);
        _exit(1);
    }

    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) { perror("child shm_open"); _exit(1); }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s == MAP_FAILED) { perror("child mmap"); _exit(1); }

    sem_t* sem_stage = open_sem_stage(idx);
    sem_t* sem_ack   = open_sem_ack(idx);
    sem_t* sem_trans = open_sem_transition();

    srand(seed ^ idx);

    while (s->running) {
        if (sem_stage) {
            sem_wait(sem_stage);
        } else {
            usleep(100000);
            continue;
        }

        if (!s->running) break;
        if (s->station_paused[idx]) continue;

        ProductInfo currentProduct;
        currentProduct.productId = 0;

        // *** FASE 1: ADQUIRIR PRODUCTO ***
        bool productAcquired = false;
        int retryCount = 0;
        const int MAX_RETRIES = 10;

        while (!productAcquired && retryCount < MAX_RETRIES) {
            if (sem_trans) sem_wait(sem_trans);

            if (idx == 0) {
                // Estación 0: verificar si slot está vacío
                if (s->product_in_station[0].productId == 0) {
                    currentProduct.productId = s->next_product_id++;
                    s->product_in_station[idx] = currentProduct;
                    productAcquired = true;
                }
            } else {
                // Otras estaciones: intentar copiar de estación anterior
                currentProduct = s->product_in_station[idx - 1];

                if (currentProduct.productId > 0) {
                    s->product_in_station[idx] = currentProduct;
                    s->product_in_station[idx - 1].productId = 0;
                    productAcquired = true;
                }
            }

            if (sem_trans) sem_post(sem_trans);

            if (!productAcquired) {
                retryCount++;
                usleep(50000);  // Esperar 50ms antes de reintentar
            }
        }

        if (!productAcquired || currentProduct.productId <= 0) {
            // No se pudo adquirir producto después de reintentos
            continue;
        }

        // *** FASE 2: PROCESAR PRODUCTO ***
        int work_ms = 800 + (rand() % 800);
        usleep(work_ms * 1000);

        // *** FASE 3: MARCAR COMO TERMINADO ***
        bool markSuccess = false;
        if (sem_trans) sem_wait(sem_trans);

        if (s->product_in_station[idx].productId == currentProduct.productId) {
            s->station_done[idx] = 1;
            markSuccess = true;
        }

        if (sem_trans) sem_post(sem_trans);

        if (!markSuccess) {
            // Producto fue sobrescrito durante el procesamiento
            continue;
        }

        // *** FASE 4: ESPERAR ACK DE LA GUI ***
        if (sem_ack) sem_wait(sem_ack);

        // *** FASE 5: TRANSFERIR A SIGUIENTE ESTACIÓN ***
        if (idx + 1 < NUM_STATIONS) {
            // Verificar que el producto aún está presente
            bool canTransfer = false;
            if (sem_trans) sem_wait(sem_trans);

            if (s->product_in_station[idx].productId == currentProduct.productId) {
                canTransfer = true;
            }

            if (sem_trans) sem_post(sem_trans);

            if (canTransfer) {
                sem_t* next = open_sem_stage(idx + 1);
                if (next) {
                    sem_post(next);
                    // CRÍTICO: Esperar tiempo suficiente para que la siguiente estación copie
                    // Con 5 productos en paralelo, necesitamos más tiempo
                    usleep(250000);  // 250ms - tiempo generoso para la copia
                }
            }
        } else {
            // Última estación: limpiar su propio slot
            if (sem_trans) sem_wait(sem_trans);
            s->product_in_station[idx].productId = 0;
            if (sem_trans) sem_post(sem_trans);
        }

        // *** FASE 6: AUTO-SEÑAL PARA ESTACIÓN 0 ***
        if (idx == 0) {
            // Delay más largo para evitar saturar el pipeline
            usleep(400000);  // 400ms - controlar tasa de producción
            if (!s->station_paused[idx] && s->running) {
                if (sem_stage) sem_post(sem_stage);
            }
        }
    }

    if (sem_trans) sem_close(sem_trans);
    munmap(s, sizeof(ShmState));
    _exit(0);
}
