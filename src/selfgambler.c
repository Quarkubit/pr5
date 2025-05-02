#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/time.h>

volatile int hidden_number;
volatile int try_count = 0;
volatile pid_t guesser_id;
volatile int min_range = 1;
volatile int max_range;
volatile int rounds_total = 10;
volatile int round_current = 0;
volatile sig_atomic_t got_guess = 0;
volatile sig_atomic_t current_try = 0;
volatile sig_atomic_t round_complete = 0;

struct timeval begin_time, finish_time;

void show_time_info() {
    gettimeofday(&finish_time, NULL);
    long secs = finish_time.tv_sec - begin_time.tv_sec;
    long usecs = finish_time.tv_usec - begin_time.tv_usec;
    double total = secs + usecs * 1e-6;
    printf("Время выполнения: %.3f секунд\n", total);
}

void correct_guess(int sig) {
    printf("\n Игрок [%d] отгадал число %d за %d попыток\n", getpid(), hidden_number, try_count);
    show_time_info();
    round_complete = 1;
    exit(0);
}

void wrong_guess(int sig) {
    printf(" Игрок [%d]: Неверно\n", getpid());
}

void process_guess(int sig, siginfo_t *info, void *context) {
    current_try = info->si_value.sival_int;
    got_guess = 1;
}

void transmit_guess(int guess) {
    union sigval value;
    value.sival_int = guess;
    if (sigqueue(guesser_id, SIGRTMIN, value)) {
        perror("Ошибка передачи числа");
        exit(1);
    }
}

void attempt_guess() {
    if (min_range > max_range || round_complete) {
        return;
    }

    int guess = min_range + (max_range - min_range) / 2;
    try_count++;
    printf("Игрок [%d] пробует число: %d\n", getpid(), guess);
    transmit_guess(guess);

    if (guess == hidden_number) {
        kill(guesser_id, SIGUSR1);
        round_complete = 1;
    } else {
        if (guess < hidden_number) {
            printf("Игрок [%d]: Загаданное число больше!\n", getpid());
            min_range = guess + 1;
        } else {
            printf("Игрок [%d]: Загаданное число меньше!\n", getpid());
            max_range = guess - 1;
        }
        kill(guesser_id, SIGUSR2);
    }
}

void guesser_role() {
    printf("\n Игрок [%d] приступил к отгадыванию\n", getpid());

    signal(SIGUSR1, correct_guess);
    signal(SIGUSR2, wrong_guess);

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = process_guess;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGRTMIN, &sa, NULL);

    while (!round_complete) {
        pause();
        if (got_guess) {
            got_guess = 0;
        }
    }
    exit(0);
}

void hider_role(int N) {
    hidden_number = rand() % N + 1;
    try_count = 0;
    min_range = 1;
    max_range = N;
    round_complete = 0;

    printf("\n    Раунд №%d    \n", round_current);
    printf(" Игрок [%d] загадал число (%d)\n", getpid(), hidden_number );
    gettimeofday(&begin_time, NULL);

    signal(SIGALRM, attempt_guess);

    while (!round_complete) {
        alarm(1);
        pause();
    }
    show_time_info();
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Формат: %s <N>\n", argv[0]);
        return 1;
    }

    int N = atoi(argv[1]);
    max_range = N;
    srand(time(NULL));

    printf(" Основной процесс [%d] начал игру\n", getpid());

    while (round_current < rounds_total) {
        round_current++;

        // Первый игрок загадывает, второй отгадывает
        pid_t player2 = fork();
        if (player2 < 0) {
            perror("Ошибка создания процесса");
            exit(1);
        }

        if (player2 == 0) {
            guesser_role();
        } else {
            guesser_id = player2;
            hider_role(N);

            kill(player2, SIGTERM);
            wait(NULL);

            // Смена ролей
            pid_t player1 = fork();
            if (player1 < 0) {
                perror("Ошибка создания процесса");
                exit(1);
            }

            if (player1 == 0) {
                guesser_role();
            } else {
                guesser_id = player1;
                hider_role(N);

                kill(player1, SIGTERM);
                wait(NULL);
            }
        }
    }

    printf("\n Основной процесс [%d] закончил игру. Всего раундов: %d.\n", getpid(), rounds_total);
    return 0;
}
