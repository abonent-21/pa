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

    int N = (argc > 1) ? atoi(argv[1]) : 3;
    float *data = NULL;
    if (rank % 2 == 0) {
        data = (float*)malloc(N * sizeof(float));
        srand(time(NULL) + rank);
        for (int i = 0; i < N; i++) {
            data[i] = (float)(rand() % 100);
        }
        // printf skipped for timing
    }

    int color = (rank % 2 == 0) ? 0 : MPI_UNDEFINED;
    MPI_Comm newcomm;
    MPI_Comm_split(comm, color, rank, &newcomm);

    if (newcomm != MPI_COMM_NULL) {
        int newrank, newsize;
        MPI_Comm_rank(newcomm, &newrank);
        MPI_Comm_size(newcomm, &newsize);

        float *min_data = NULL;
        if (newrank == 0) min_data = (float*)malloc(N * sizeof(float));

        double start_time, end_time;
        if (newrank == 0) start_time = MPI_Wtime();

        // For accurate measurement, loop if N small
        int iterations = (N < 1000) ? 10000 : 1; // Adjust
        for (int it = 0; it < iterations; it++) {
            MPI_Reduce((rank % 2 == 0) ? data : NULL, min_data, N, MPI_FLOAT, MPI_MIN, 0, newcomm);
        }

        if (newrank == 0) {
            end_time = MPI_Wtime();
            double avg_time = (end_time - start_time) / iterations;
            printf("Среднее время редукции: %f секунд\n", avg_time);
            // Print min_data if needed
        }

        if (newrank == 0) free(min_data);
    }

    if (rank % 2 == 0) free(data);
    if (newcomm != MPI_COMM_NULL) MPI_Comm_free(&newcomm);

    MPI_Finalize();
    return 0;
}
