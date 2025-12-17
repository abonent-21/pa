#include <stdio.h>
#include <mpi.h>

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Comm comm_cart; // Новый коммуникатор для декартовой топологии
    int dims[2];        // Размеры решетки (2xN)
    int periods[2];     // Периодичность (только по строкам)
    int reorder = 0;    // Не переупорядочивать ранги
    int my_coords[2];   // Координаты процесса в решетке
    int rank_source, rank_dest; // Ранги для отправки и получения
    
    double A_send; // Исходное число A
    double A_recv; // Полученное число A

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 1. Проверка условия: K = 2N, N > 1 (т.е. K - четное и K > 2)
    if (size % 2 != 0 || size <= 2) {
        if (rank == 0) {
            fprintf(stderr, "Ошибка: Число процессов K=%d должно быть четным и > 2 (K=2N, N>1).\n", size);
        }
        MPI_Finalize();
        return 1;
    }

    int N = size / 2;
    
    // Инициализируем исходные данные (для примера возьмем (double)rank)
    A_send = (double)rank;

    // 2. Определение декартовой топологии 2xN
    dims[0] = 2;
    dims[1] = N;

    // Строки (измерение 1) являются циклическими, столбцы (измерение 0) - нет
    periods[0] = 0;
    periods[1] = 1; 

    // Создаем новую декартову топологию
    MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, &comm_cart);

    // Получаем свои координаты (для наглядности)
    MPI_Cart_coords(comm_cart, rank, 2, my_coords);

    // 3. Определение рангов для циклического сдвига
    // Мы сдвигаем по измерению 1 (строки) на 1 позицию
    // disp = 1: 
    //   rank_source - это ранг процесса (coord[1] - 1)
    //   rank_dest - это ранг процесса (coord[1] + 1)
    MPI_Cart_shift(comm_cart, 1, 1, &rank_source, &rank_dest);

    // 4. Выполнение пересылки с помощью MPI_Sendrecv
    // Каждый процесс отправляет свое A процессу 'rank_dest'
    // и получает новое A от процесса 'rank_source'.
    MPI_Sendrecv(
        &A_send, 1, MPI_DOUBLE, rank_dest, 0,
        &A_recv, 1, MPI_DOUBLE, rank_source, 0,
        comm_cart, MPI_STATUS_IGNORE
    );

    // 5. Вывод полученных данных (с упорядочиванием)
    MPI_Barrier(comm_cart); // Синхронизация перед выводом
    
    int i;
    for (i = 0; i < size; i++) {
        if (rank == i) {
            printf("Ранг %d (Коорд: [%d, %d]): Исходное A = %.1f, Полученное A = %.1f (от Ранга %d)\n",
                   rank, my_coords[0], my_coords[1], A_send, A_recv, rank_source);
        }
        // Барьер нужен, чтобы вывод не перемешивался
        MPI_Barrier(comm_cart); 
    }

    // Освобождаем созданный коммуникатор
    MPI_Comm_free(&comm_cart);
    
    MPI_Finalize();
    return 0;
}
