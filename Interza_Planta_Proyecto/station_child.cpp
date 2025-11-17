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
    // open ipc resources created by parent
    if (!open_ipc()) {
        fprintf(stderr, "Child %d: cannot open ipc\n", idx);
        _exit(1);
    }

    // map shared memory pointer (ipc_common keeps internal mapping)
    int fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (fd == -1) { perror("child shm_open"); _exit(1); }
    ShmState* s = (ShmState*)mmap(NULL, sizeof(ShmState), PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (s == MAP_FAILED) { perror("child mmap"); _exit(1); }

    sem_t* sem_stage = open_sem_stage(idx);
    sem_t* sem_ack   = open_sem_ack(idx);

    srand(seed ^ idx);

    while (s->running) {
        // respect pause flag
        if (s->station_paused[idx]) {
            usleep(200000);
            continue;
        }

        // wait for stage semaphore (permission to start this piece)
        if (sem_stage) {
            sem_wait(sem_stage);
        } else {
            usleep(100000);
            continue;
        }

        if (!s->running) break;



        // --- INICIO DE LA LÓGICA DE PERSISTENCIA ---

        //Determinar el ID del producto actual
        ProductInfo currentProduct;
        if (idx == 0) {
            // Estación 0: Crea un nuevo producto. Toma el siguiente ID disponible
            // de la memoria compartida y lo incrementa para la próxima vez.
            // Esto necesita protección si múltiples procesos lo modifican,
            // pero como solo la estación 0 lo hace, es seguro.
            currentProduct.productId = s->next_product_id++;
        } else {
            // Demás estaciones: Toman la información del producto de la estación anterior.
            currentProduct = s->product_in_station[idx - 1];
        }

        //Registra el producto actual en la memoria compartida
        s->product_in_station[idx] = currentProduct;

        // --- FIN DE PERSISTENCIA ---


        int work_ms = 800 + (rand() % 800);
        usleep(work_ms * 1000);


        s->station_done[idx] = 1;


        if (sem_ack) {
            sem_wait(sem_ack);
        }

        // H. Pasar el relevo a la siguiente estación (si no es la última)
        if (idx + 1 < NUM_STATIONS) {
            sem_t* next = open_sem_stage(idx + 1);
            if (next) sem_post(next);
        }

        //Estación 0 debe auto-reactivarse para crear un nuevo producto
        // y mantener la línea de producción alimentada.
        if (idx == 0) {
            if (sem_stage) sem_post(sem_stage);
        }

        // Limpiar el ID del producto de la estación anterior para evitar "productos fantasma"
        // al guardar. Una vez que el producto ha sido pasado, el slot anterior queda libre.
        // La estación actual (idx) se limpia antes de que la siguiente (idx+1) tome el producto.
        if (idx > 0) {
            s->product_in_station[idx - 1].productId = 0; // Marcar como vacío
        }
    }

    // se sale
    munmap(s, sizeof(ShmState));
    _exit(0);
}
