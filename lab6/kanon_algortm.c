#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <time.h> 
#include <mpi.h>

// Последовательное умножение (для проверки)
void serial_multiply(int n, int *A, int *B, int *C) {
    for (int i = 0; i < n * n; i++) C[i] = 0;
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < n; k++) {
            int temp = A[i * n + k];
            for (int j = 0; j < n; j++) {
                C[i * n + j] += temp * B[k * n + j];
            }
        }
    }
}

// Локальное умножение блоков
void matrix_multiply_add(int n, int *A, int *B, int *C) {
    for (int i = 0; i < n; i++) {
        for (int k = 0; k < n; k++) {
            int temp = A[i * n + k];
            for (int j = 0; j < n; j++) {
                C[i * n + j] += temp * B[k * n + j];
            }
        }
    }
}

// Строки -> Блоки
void convert_to_blocks(int *input, int *output, int N, int grid_dim) {
    int block_size = N / grid_dim;
    int idx = 0;
    for (int bi = 0; bi < grid_dim; bi++) {
        for (int bj = 0; bj < grid_dim; bj++) {
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    int global_row = bi * block_size + i;
                    int global_col = bj * block_size + j;
                    output[idx++] = input[global_row * N + global_col];
                }
            }
        }
    }
}

// Блоки -> Строки
void convert_from_blocks(int *input, int *output, int N, int grid_dim) {
    int block_size = N / grid_dim;
    int idx = 0;
    for (int bi = 0; bi < grid_dim; bi++) {
        for (int bj = 0; bj < grid_dim; bj++) {
            for (int i = 0; i < block_size; i++) {
                for (int j = 0; j < block_size; j++) {
                    int global_row = bi * block_size + i;
                    int global_col = bj * block_size + j;
                    output[global_row * N + global_col] = input[idx++];
                }
            }
        }
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int dims[2] = {0, 0};
    MPI_Dims_create(size, 2, dims);
    if (dims[0] != dims[1]) {
        if (rank == 0) fprintf(stderr, "Ошибка: Число процессов P=%d должно быть квадратом (1, 4, 16...)\n", size);
        MPI_Finalize();
        return 1;
    }
    int sqrt_p = dims[0];

    int N;
    if (rank == 0) {
        printf("Введите размер матриц N (для NxN): ");
        fflush(stdout);
        if (scanf("%d", &N) != 1) N = 0;
        
        if (N % sqrt_p != 0) {
            fprintf(stderr, "Ошибка: N=%d должно делиться на sqrt(P)=%d.\n", N, sqrt_p);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
    }
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int block_n = N / sqrt_p;
    int block_size = block_n * block_n;

    int *A_serial = NULL, *B_serial = NULL, *C_serial = NULL;
    int *A_blocked = NULL, *B_blocked = NULL, *C_blocked = NULL;
    int *C_final = NULL;

    if (rank == 0) {
        srand(time(NULL)); // Инициализация рандома

        A_serial = (int*)malloc(N * N * sizeof(int));
        B_serial = (int*)malloc(N * N * sizeof(int));
        C_serial = (int*)malloc(N * N * sizeof(int));
        
        printf("Генерация случайных матриц A и B размером %dx%d...\n", N, N);
        for (int i = 0; i < N * N; i++) {
            A_serial[i] = rand() % 10; // Случайные числа 0-9
            B_serial[i] = rand() % 10;
        }

        // Последовательный расчет для проверки
        printf("Запуск последовательного алгоритма (для проверки)...\n");
        double t_start = MPI_Wtime();
        serial_multiply(N, A_serial, B_serial, C_serial);
        double t_end = MPI_Wtime();
        printf("Время последовательного: %f сек.\n", t_end - t_start);

        // Подготовка к параллельному
        A_blocked = (int*)malloc(N * N * sizeof(int));
        B_blocked = (int*)malloc(N * N * sizeof(int));
        C_blocked = (int*)malloc(N * N * sizeof(int));
        
        convert_to_blocks(A_serial, A_blocked, N, sqrt_p);
        convert_to_blocks(B_serial, B_blocked, N, sqrt_p);
    }

    MPI_Comm grid_comm;
    int periods[2] = {1, 1};
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, 1, &grid_comm);

    int coords[2];
    MPI_Cart_coords(grid_comm, rank, 2, coords);

    int *loc_A = (int*)malloc(block_size * sizeof(int));
    int *loc_B = (int*)malloc(block_size * sizeof(int));
    int *loc_C = (int*)calloc(block_size, sizeof(int));

    MPI_Scatter(A_blocked, block_size, MPI_INT, loc_A, block_size, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Scatter(B_blocked, block_size, MPI_INT, loc_B, block_size, MPI_INT, 0, MPI_COMM_WORLD);

    // --- CANNON ALGORITHM ---
    MPI_Barrier(MPI_COMM_WORLD);
    double para_start = MPI_Wtime();

    int shift_src, shift_dst;
    
    // Initial Skewing
    if (coords[0] > 0) {
        MPI_Cart_shift(grid_comm, 0, -coords[0], &shift_src, &shift_dst);
        MPI_Sendrecv_replace(loc_A, block_size, MPI_INT, shift_dst, 1, shift_src, 1, grid_comm, MPI_STATUS_IGNORE);
    }
    if (coords[1] > 0) {
        MPI_Cart_shift(grid_comm, 1, -coords[1], &shift_src, &shift_dst);
        MPI_Sendrecv_replace(loc_B, block_size, MPI_INT, shift_dst, 1, shift_src, 1, grid_comm, MPI_STATUS_IGNORE);
    }

    int left, right, up, down;
    MPI_Cart_shift(grid_comm, 0, -1, &right, &left);
    MPI_Cart_shift(grid_comm, 1, -1, &down, &up);

    for (int k = 0; k < sqrt_p; k++) {
        matrix_multiply_add(block_n, loc_A, loc_B, loc_C);
        MPI_Sendrecv_replace(loc_A, block_size, MPI_INT, left, 1, right, 1, grid_comm, MPI_STATUS_IGNORE);
        MPI_Sendrecv_replace(loc_B, block_size, MPI_INT, up, 2, down, 2, grid_comm, MPI_STATUS_IGNORE);
    }

    MPI_Barrier(MPI_COMM_WORLD);
    double para_end = MPI_Wtime();

    MPI_Gather(loc_C, block_size, MPI_INT, C_blocked, block_size, MPI_INT, 0, MPI_COMM_WORLD);

    if (rank == 0) {
        printf("Время параллельного (MPI Cannon): %f сек.\n", para_end - para_start);

        C_final = (int*)malloc(N * N * sizeof(int));
        convert_from_blocks(C_blocked, C_final, N, sqrt_p);

        // Проверка
        int errors = 0;
        for (int i = 0; i < N * N; i++) {
            if (C_serial[i] != C_final[i]) errors++;
        }

        if (errors == 0) printf(">> Результат верный (ошибок нет).\n");
        else printf(">> ОШИБКА: Найдено %d несовпадений!\n", errors);

        // Вывод если N мало
        if (N <= 10) {
            printf("\nМатрица A:\n");
            for(int i=0;i<N;i++) { for(int j=0;j<N;j++) printf("%d ", A_serial[i*N+j]); printf("\n"); }
            printf("\nМатрица B:\n");
            for(int i=0;i<N;i++) { for(int j=0;j<N;j++) printf("%d ", B_serial[i*N+j]); printf("\n"); }
            printf("\nРезультат C:\n");
            for(int i=0;i<N;i++) { for(int j=0;j<N;j++) printf("%d ", C_final[i*N+j]); printf("\n"); }
        }

        free(A_serial); free(B_serial); free(C_serial);
        free(A_blocked); free(B_blocked); free(C_blocked);
        free(C_final);
    }

    free(loc_A); free(loc_B); free(loc_C);
    MPI_Finalize();
    return 0;
}
