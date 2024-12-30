#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include "helper.h"

volatile sig_atomic_t fire_alarm = 0;

int arg_checker(int argc, char* argv[]);
void fire_signal_handler(int sig);

int main(int argc, char* argv[]) {
       	int table_count = arg_checker(argc, argv);
	int manager_id = getpid();
	srand(time(NULL));

	// Obsluga sygnalu dla menedzera
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = fire_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction error [manager]");
		exit(1);
	}

	// Tworzenie + odpalanie kasjera c;
	pid_t cashier_id = fork();
	if (cashier_id == -1) {
		perror("blad fork() [tworzenie kasjera]");
		exit(1);
	}

	if (cashier_id == 0) {
		execl("./cashier", "cashier", argv[1], argv[2], argv[3], argv[4], NULL);
		perror("Manager: nie udalo sie odpalic procesu kasjera");
		exit(1);
	}

	sleep(2); 

	// Tworzenie + odpalanie strazaka c;
	pid_t fireman_id = fork();
	if (fireman_id == -1) {
		perror("blad fork() [tworzenie strazaka]");
		exit(1);
	}

	 if (fireman_id == 0) {
                char pid_cashier[50];
                char pid_manager[50];
                char table_cnt[50];

                snprintf(pid_cashier, sizeof(pid_cashier), "%d", cashier_id);
                snprintf(pid_manager, sizeof(pid_manager), "%d", manager_id);
                snprintf(table_cnt, sizeof(table_cnt), "%d", table_count);

                execl("./fireman", "fireman", pid_cashier, pid_manager, table_cnt, NULL);
                perror("Manager: nie udalo sie odpalic procesu strazaka");
                exit(1);
        }

	// Dostep do semaforow + pamieci dzielonej

	key_t shm_key = ftok(".", SHM_GEN_KEY);
	if (shm_key == -1) {
		perror("Blad generowania klucza w ftok()");
		exit(1);
	}

	key_t sem_key = ftok(".", SEM_GEN_KEY);
	if (sem_key == -1) {
		perror("Blad generowania klucza w ftok()");
		exit(1);
	}

	int sem_id = join_sem(sem_key);
	int shm_id = join_shm(shm_key);

	Table* tables = shmat(shm_id, NULL, 0);
	if (tables == (void*)-1) {
		perror("Blad podlaczenia pamieci dzielonej");
		exit(1);
	}
		
	// TODO - task z pameicia dzielona dla managera
	
	
	// Tworzenie klientow
	while (kill(cashier_id, 0) == 0 && !fire_alarm) {
		char group_size[50];
		int size = rand() % 3 + 1;
		snprintf(group_size, sizeof(group_size), "%d", size);

		pid_t client_id = fork();
		if (client_id == -1) {
			perror("blad fork() [tworzenie klienta])");
			exit(1);
		}

		if (client_id == 0) {
			execl("./client", "client", group_size, NULL);
			perror("Manager: nie udalo sie odpalic procesu klienta");
			exit(1);
		}
		sleep(rand() % 10 + 5);
	}

	while (wait(NULL) > 0);

	remove_shm(shm_id, tables);
	remove_sem(sem_id);

	printf("Manager: konczymy zabawe!\n");

	return 0;
}

int arg_checker(int argc, char* argv[]) {
        if (argc != 5) {
                fprintf(stderr, "Bledna liczba argumentow. Poprawne uzycie ./manager <X1> <X2> <X3> <X4>");
                exit(1);
        }

        int x1 = atoi(argv[1]);
        int x2 = atoi(argv[2]);
        int x3 = atoi(argv[3]);
        int x4 = atoi(argv[4]);

        if ((x1 < 0 || x2 < 0 || x3 < 0 || x4 < 0) || (x1 == 0 && x2 == 0 && x3 == 0 && x4 == 0)) {
                fprintf(stderr, "Niepoprawne argumenty.\n1. Kazdy z argumentow musi byc nieujemny!\n2. Co najmniej jeden argument musi byc rozny od zera!");
                exit(1);
        }

	return x1+x2+x3+x4;
}

void fire_signal_handler(int sig) {
	if (sig == SIGUSR1) {
		fire_alarm = 1;		
	}
	printf("Manager: otrzymano sygnal");
}
