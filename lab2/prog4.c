#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h> 

#define MSG_TAG 0         // Тег для обычных данных
#define TERMINATE_TAG 1 // Тег для сигнала "завершить работу"

// Определяем структуру нашего сообщения
typedef struct {
    int src;    // Ранг (ID) создателя
    int dest;   // Ранг (ID) получателя
    int ttl;    // Time-To-Live (время жизни)
    int data;   // Полезные данные
} Message;

// Главная функция программы
int main(int argc, char** argv) {
    // Основные переменные MPI
    int rank, size;
    MPI_Status status; // Структура для получения статуса (от кого, какой тег)

    // Инициализация MPI
    MPI_Init(&argc, &argv);
    // Получаем ранг (ID) текущего процесса
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    // Получаем общее количество процессов
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Значения по умолчанию
    int num_messages = 5;
    int max_ttl = 10;
    int verbose = 0; // 0 = тихий режим, 1 = подробный

    // Парсинг аргументов командной строки (если они есть)
    if (argc > 1) num_messages = atoi(argv[1]); // 1-й аргумент - кол-во сообщений
    if (argc > 2) max_ttl = atoi(argv[2]);      // 2-й аргумент - TTL
    if (argc > 3) verbose = atoi(argv[3]);      // 3-й аргумент - подробный режим

    // Инициализация генератора случайных чисел
    // гарантирует, что каждый параллельный процесс будет генерировать свою собственную, уникальную последовательность случайных чисел. time(null)
    // + rank - уникальный посев
    srand(time(NULL) + rank);

    // Ранг следующего процесса (с "замыканием" size-1 -> 0)
    int next = (rank + 1) % size;
    // Ранг предыдущего процесса (с "замыканием" 0 -> size-1)
    int prev = (rank - 1 + size) % size;

    Message msg;      // Буфер для одного сообщения
    int done = 0;     // Флаг выхода из главного цикла (1 = выходим)
    
    double start_time = 0.0, end_time = 0.0; // Для замера времени
    double last_msg_time = 0.0; // Таймер для "детектора тишины" помогает процессу 0 понять что все сообщения обработаны

    if (rank == 0) {
        printf("=== Имитация кольцевой топологии ===\n");
        printf("Процессов: %d\n", size);
        printf("Сообщений на процесс: %d\n", num_messages);
        printf("TTL сообщений: %d\n", max_ttl);
        printf("====================================\n");
        // Только процесс 0 засекает общее время
        start_time = MPI_Wtime();
    }
    
    // Инициализируем таймер таймаута для ВСЕХ процессов
    // Это предотвращает ложное срабатывание таймаута до начала симуляции
    last_msg_time = MPI_Wtime();

    // Каждый процесс "вбрасывает" в кольцо свои сообщения
    for (int i = 0; i < num_messages; i++) {
        msg.src = rank;                     // Отправитель - я
        msg.dest = rand() % size;           // Получатель - случайный
        msg.ttl = max_ttl;                  // Ставим TTL
        msg.data = rand() % 1000;           // Случайные данные

        // Печатаем, если включен подробный режим
        if (verbose)
            printf("Процесс %d создал сообщение для %d (data=%d)\n", rank, msg.dest, msg.data);

        // Отправляем сообщение СЛЕДУЮЩЕМУ в кольце не адресату
        MPI_Send(&msg, sizeof(Message), MPI_BYTE, next, MSG_TAG, MPI_COMM_WORLD);
    }

    while (!done) {
        int flag = 0; // Флаг: есть ли сообщение? (0=нет, 1=да)

        
        // MPI_Iprobe "смотрит" в очередь, не зависая, есть ли что-то от 'prev'
        MPI_Iprobe(prev, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &status);

        // Если сообщение ЕСТЬ (flag = 1)
        if (flag) {
            // Раз мы что-то получили, значит, сеть активна.
            // Сбрасываем таймер "детектора тишины"
            last_msg_time = MPI_Wtime();

            // Проверяем ТЕГ сообщения (до того, как его приняли)
	    //Этот код проверяет тег сообщения до того, как его принять. Это позволяет процессу отличить обычное сообщение с данными (MSG_TAG) от 			специальной команды (сигнала TERMINATE_TAG), чтобы понять, нужно ли обрабатывать данные или пора завершать работу.
            if (status.MPI_TAG == TERMINATE_TAG) {
                
                // Мы должны принять сигнал (даже если он пустой),
                // чтобы он исчез из очереди.
                MPI_Recv(NULL, 0, MPI_BYTE, prev, TERMINATE_TAG, MPI_COMM_WORLD, &status);
                
                done = 1; // Ставим флаг "завершаемся"
                
                // Пересылаем сигнал дальше по кольцу
                // (rank != next - защита для случая с 1 процессом)
                if (rank != next) 
                    MPI_Send(NULL, 0, MPI_BYTE, next, TERMINATE_TAG, MPI_COMM_WORLD);
                
                continue; // Переходим к следующей итерации (в !done будет 0)
            }

            // Если это не сигнал, значит, это обычное сообщение
            // Теперь мы его принимаем (блокирующе, но мы знаем, что оно есть)
            MPI_Recv(&msg, sizeof(Message), MPI_BYTE, prev, MSG_TAG, MPI_COMM_WORLD, &status);

            // Обрабатываем сообщение
            msg.ttl--; // Уменьшаем TTL

            // Проверяем TTL
            if (msg.ttl <= 0) {
                // TTL истек. Сообщение "удаляется" (просто не пересылается)
                if (rank == 0 && verbose) // Контроллер может об этом сообщить
                    printf("Контроллер (0): удалил сообщение от %d к %d (TTL истёк)\n", msg.src, msg.dest);
                continue; // Переходим к след. итерации
            }

            // Проверяем адресата
            if (msg.dest == rank) {
                // Сообщение пришло МНЕ
                if (verbose)
                    printf("Процесс %d получил сообщение от %d (data=%d, ttl=%d)\n",
                           rank, msg.src, msg.data, msg.ttl);
            } else {
                // Сообщение ЧУЖОЕ - пересылаем дальше
                MPI_Send(&msg, sizeof(Message), MPI_BYTE, next, MSG_TAG, MPI_COMM_WORLD);
            }
        } 

        
        // Логика "детектора тишины" (только у Контроллера 0)
        if (rank == 0) {
            // Проверяем, как давно была последняя активность (получение)
            if (MPI_Wtime() - last_msg_time > 2.0) { // Таймаут = 2 секунды
                
                if (!done) { // Отправляем сигнал только один раз
                    if (verbose)
                        printf("Контроллер (0): таймаут (тишина в сети), рассылаем сигнал завершения\n");
                    
                    // Отправляем сигнал СЛЕДУЮЩЕМУ
                    MPI_Send(NULL, 0, MPI_BYTE, next, TERMINATE_TAG, MPI_COMM_WORLD);
                    
                    // Контроллер 0 сам себя тоже должен остановить
                    done = 1;
                }
            }
        } 

        // Небольшая пауза (100 микросекунд), чтобы цикл 'while'
        // не загружал CPU на 100%, пока ждет сообщений (в 'else')
        usleep(100);
    } // --- Конец while(!done) ---

    
    
    // Ждем, пока ВСЕ процессы выйдут из цикла 'while'
    MPI_Barrier(MPI_COMM_WORLD);

    // Только процесс 0 печатает итог
    if (rank == 0) {
        end_time = MPI_Wtime();
        double elapsed = end_time - start_time;

        printf("\n=== РЕЗУЛЬТАТ ===\n");
        printf("Процессов: %d\n", size);
        printf("Сообщений/процесс: %d\n", num_messages);
        printf("TTL сообщений: %d\n", max_ttl);
        printf("Время выполнения: %.6f секунд\n", elapsed);
    }

    
    MPI_Finalize();
    return 0; 
}
