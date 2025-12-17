#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Comm comm_cart;
    int dims[2], periods[2], my_coords[2];
    int rank_source, rank_dest;
    
    double t_start, t_end, t_max;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Проверка на четность
    if (size % 2 != 0 || size <= 2) {
        if (rank == 0) fprintf(stderr, "Ошибка: K должно быть четным и > 2.\n");
        MPI_Finalize();
        return 1;
    }

    // Создание топологии (как в задании)
    int N = size / 2;
    dims[0] = 2; dims[1] = N;
    periods[0] = 0; periods[1] = 1; 
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 0, &comm_cart);
    MPI_Cart_shift(comm_cart, 1, 1, &rank_source, &rank_dest);

    // --- ВХОДНЫЕ ДАННЫЕ: Размеры сообщений для теста ---
    // Мы проверим передачу 1 элемента, 1000, 100 тыс. и 10 млн. элементов
    int sizes[] = {1, 1000, 100000, 1000000, 10000000};
    int num_tests = 5; 

    if (rank == 0) {
        printf("   Размер (double) |   Память (КБ)   |   Время (сек)  \n");
        printf("-------------------|-----------------|----------------\n");
    }

    for (int i = 0; i < num_tests; i++) {
        int count = sizes[i]; // Текущая длина сообщения (Входные данные)
        
        // 1. Выделение памяти под массивы нужного размера
        double *buffer_send = (double*)malloc(count * sizeof(double));
        double *buffer_recv = (double*)malloc(count * sizeof(double));

        // Заполнение данными (для теста)
        for (int j = 0; j < count; j++) {
            buffer_send[j] = (double)rank + 0.1 * j;
        }

        // Синхронизация перед замером
        MPI_Barrier(comm_cart);
        t_start = MPI_Wtime();

        // 2. ПЕРЕДАЧА ДАННЫХ (Один раз, но большого объема)
        MPI_Sendrecv(
            buffer_send, count, MPI_DOUBLE, rank_dest, 0,
            buffer_recv, count, MPI_DOUBLE, rank_source, 0,
            comm_cart, MPI_STATUS_IGNORE
        );

        t_end = MPI_Wtime();

        // 3. Сбор максимального времени
        double t_local = t_end - t_start;
        MPI_Reduce(&t_local, &t_max, 1, MPI_DOUBLE, MPI_MAX, 0, comm_cart);

        if (rank == 0) {
            double size_kb = (count * sizeof(double)) / 1024.0;
            printf(" %9d элам.   | %10.2f КБ   |  %.6f \n", count, size_kb, t_max);
        }

        // Очистка памяти перед следующим тестом
        free(buffer_send);
        free(buffer_recv);
    }

    MPI_Comm_free(&comm_cart);
    MPI_Finalize();
    return 0;
}
