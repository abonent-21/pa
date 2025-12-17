#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm comm = MPI_COMM_WORLD;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    float data[3] = {0.0, 0.0, 0.0};
    if (rank % 2 == 0) {
        // Инициализация данных (для примера используем случайные значения)
        srand(time(NULL) + rank);
        data[0] = (float)(rand() % 100);
        data[1] = (float)(rand() % 100);
        data[2] = (float)(rand() % 100);
        printf("Процесс %d: %.2f %.2f %.2f\n", rank, data[0], data[1], data[2]);
    }

    // Создание нового коммуникатора
    int color = (rank % 2 == 0) ? 0 : MPI_UNDEFINED;
    int key = rank;
    MPI_Comm newcomm;
    MPI_Comm_split(comm, color, key, &newcomm);

    if (newcomm != MPI_COMM_NULL) {
        int newrank;
        MPI_Comm_rank(newcomm, &newrank);

        float min_data[3];
        // Коллективная редукция для нахождения минимумов
        MPI_Reduce(data, min_data, 3, MPI_FLOAT, MPI_MIN, 0, newcomm);

        if (newrank == 0) {
            printf("Минимальные значения: %.2f %.2f %.2f\n", min_data[0], min_data[1], min_data[2]);
        }
    }

    if (newcomm != MPI_COMM_NULL) {
        MPI_Comm_free(&newcomm);
    }

    MPI_Finalize();
    return 0;
}
