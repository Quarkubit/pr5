#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>

#define MAX_ROUNDS 10
#define GUESS_SIGNAL SIGRTMIN
#define START_SIGNAL (SIGRTMIN + 1)

int N;
pid_t other_pid;
volatile sig_atomic_t secret;
volatile sig_atomic_t guess;
volatile sig_atomic_t round_ready = 0;

// Обработчик для сигнала догадки
void handle_guess(int sig, siginfo_t *info, void *context) {
    (void)sig; (void)context;
    guess = info->si_value.sival_int;
}

// Обработчик для стартового сигнала
void handle_start(int sig) {
    (void)sig;
    round_ready = 1;
}

void host_mode() {
    struct sigaction sa;
    sa.sa_sigaction = handle_guess;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(GUESS_SIGNAL, &sa, NULL);

    secret = rand() % N + 1;
    printf("[HOST %d] Загадал: %d\n", getpid(), secret);

    // Отправляем стартовый сигнал и ждем подтверждения
    kill(other_pid, START_SIGNAL);

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int attempts = 0;
    while (1) {
        pause(); // Ждем сигнал GUESS_SIGNAL
        attempts++;

        if (guess == secret) {
            printf("[HOST] Угадано за %d попыток!\n", attempts);
            kill(other_pid, SIGUSR1);
            break;
        } else {
            kill(other_pid, SIGUSR2);
        }
    }
}

void guesser_mode() {
    struct sigaction sa;
    sa.sa_handler = handle_start;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(START_SIGNAL, &sa, NULL);

    // Ждем стартовый сигнал
    while (!round_ready)
        pause();
    round_ready = 0;

    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigaddset(&mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    int attempts = 0;
    for (int g = 1; g <= N; ++g) {
        union sigval val = {.sival_int = g};
        sigqueue(other_pid, GUESS_SIGNAL, val);
        attempts++;

        int received_sig;
        sigwait(&mask, &received_sig);

        if (received_sig == SIGUSR1) {
            printf("[GUESSER] Успех! Число %d найдено за %d попыток\n", g, attempts);
            return;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2 || (N = atoi(argv[1])) <= 0) {
        fprintf(stderr, "Использование: %s <N>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    srand(time(NULL));

    // Блокируем сигналы для избежания гонок
    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, GUESS_SIGNAL);
    sigaddset(&block_mask, START_SIGNAL);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid > 0) { // Родитель
        other_pid = pid;
        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
        for (int i = 0; i < MAX_ROUNDS; i++) {
            host_mode();
            guesser_mode();
        }
        kill(pid, SIGTERM); // Завершаем дочерний процесс
    } else { // Дочерний
        other_pid = getppid();
        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);
        for (int i = 0; i < MAX_ROUNDS; i++) {
            guesser_mode();
            host_mode();
        }
    }

    return 0;
}
