#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_GAMES 10

volatile sig_atomic_t targ;
volatile sig_atomic_t guess;
volatile sig_atomic_t player;
volatile sig_atomic_t finish;
volatile sig_atomic_t attempts;

void p1_handler(int sig, siginfo_t* info, void* context) {
    (void)context;
    if (sig == SIGUSR1) {
        guess = info->si_value.sival_int;
        if (guess == targ) {
            kill(info->si_pid, SIGUSR1);
            finish = 1;
        } else {
            kill(info->si_pid, SIGUSR2);
        }
        attempts++;
    }
}

void p2_handler(int sig, siginfo_t* info, void* context) {
    (void)info; (void)context;
    if (sig == SIGUSR1) {
        finish = 1;
    }
}

void sending_guess(pid_t pid, int value) {
    union sigval val;
    val.sival_int = value;
    sigqueue(pid, SIGUSR1, val);
}

void handler_setup(void (*handler)(int, siginfo_t*, void*), int sig) {
    struct sigaction sa;
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGUSR1);
    sigaddset(&sa.sa_mask, SIGUSR2);
    sigaction(sig, &sa, NULL);
}

void _game(int N, int cur_player) {
    pid_t pid = getpid();
    pid_t ppid = cur_player ? player : getppid();

    if (cur_player) {
        targ = rand() % N + 1;
        printf("P1 (PID %d) загадал: %d\n", pid, targ);

        finish = 0;
        attempts = 0;

        kill(ppid, SIGUSR1);

        while(!finish) pause();

        printf("Число %d угадано за %d попыток\n", targ, attempts);
    } else {
        int cur_guess;
        finish = 0;

        while(!finish) {
            cur_guess = rand() % N + 1;
            printf("P2 (PID %d) пробует: %d\n", pid, cur_guess);
            sending_guess(ppid, cur_guess);
            pause();
        }
    }
}

int set_N(int arg, char* argv[]) {
    int N = 0;
    if (arg <= 1) {
        printf("Введите N: ");
        scanf("%d", &N);
    } else {
        N = atoi(argv[1]);
        printf("Используется N = %d\n", N);
    }
    return N > 0 ? N : 100;
}

int main(int argc, char* argv[]) {
    int N = set_N(argc, argv);
    srand(time(NULL));

    sigset_t block_mask;
    sigemptyset(&block_mask);
    sigaddset(&block_mask, SIGUSR1);
    sigaddset(&block_mask, SIGUSR2);
    sigprocmask(SIG_BLOCK, &block_mask, NULL);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (pid == 0) { // Дочерний процесс
        handler_setup(p2_handler, SIGUSR1);
        handler_setup(p2_handler, SIGUSR2);
        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

        for (int i = 0; i < MAX_GAMES; i++) {
            _game(N, 0);
        }
        exit(EXIT_SUCCESS);
    } else { // Родительский процесс
        player = pid;
        handler_setup(p1_handler, SIGUSR1);
        handler_setup(p1_handler, SIGUSR2);
        sigprocmask(SIG_UNBLOCK, &block_mask, NULL);

        for (int i = 0; i < MAX_GAMES; i++) {
            _game(N, 1);
        }

        kill(pid, SIGTERM);
        wait(NULL);
    }

    return 0;
}
