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
#include <fcntl.h>
#include <errno.h>
#include "helper.h"

volatile sig_atomic_t fire_alarm = 0;

int arg_checker(int argc, char* argv[]);
void fire_signal_handler(int sig);
void read_report();

int main(int argc, char* argv[]) {
       	int table_count = arg_checker(argc, argv);
	int manager_id = getpid();
	int sigusr2_sent = 0;
	srand(time(NULL));

	// Obsluga sygnalu dla menedzera
	struct sigaction sa;
	sa.sa_handler = fire_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction error [manager]");
		exit(1);
	}

	// Tworzenie zbioru semaforow, dla managera potrzebny jest tylko po to, zeby strazak zaczal dzialac, gdy juz kasjer zaladuje potrzebne zasoby :)
	key_t sem_key = ftok(".", SEM_GEN_KEY);
	if (sem_key == -1) {
		perror("Blad generowania klucza w ftok()");
		exit(1);
	}

	int sem_id = create_sem(sem_key);
		
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

	P(sem_id, SEM_INIT_READY); // czekamy, az kasjer sie 'zaladuje' i pozwoli nam zaladowac strazaka, pozniej ten semafor juz jest zbedny

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

	// ile +/- dziala restauracja 
	unsigned long worktime = time(NULL) + WORK_TIME;
	int active_clients = 0;	

	while (!fire_alarm && (!sigusr2_sent || (unsigned long)time(NULL) < worktime)) {
		char group_size[5];
		int rand_group_size = rand() % 3 + 1;
		snprintf(group_size, sizeof(group_size), "%d", rand_group_size);
		
		if (active_clients < MAX_ACTIVE_CLIENTS && !fire_alarm) {
		       	active_clients++;	
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
		}
		// co ile generujemy klientow
		int generate_time = (rand() % 1001 + 500) * 1000;
		usleep(generate_time);

		if (!sigusr2_sent && (worktime - TIME_TO_CLOSE < (unsigned long)time(NULL))) {
			sigusr2_sent = 1;
			printf("\033[41mManager: Kasjer, sluchaj niedlugo zamykamy, nie wpuszczaj juz klientow.\033[0m\n");
			if (kill(cashier_id, SIGUSR2) == -1) {
				perror("Nie udalo sie wyslac SIGUSR2 do kasjera!");
				exit(1);
			}
		}
	
		// zbieramy zombiaki
		while (waitpid(-1, NULL, WNOHANG) > 0)
			active_clients--;
	}

	while (waitpid(cashier_id, NULL, 0) == -1) {
		if (errno == EINTR)
			continue;
		else if (errno == ECHILD)
			break;
		else {
			perror("Blad waitpid");
			break;
		}
	}

	if (!fire_alarm && kill(cashier_id, 0) != 0) {
		if (kill(fireman_id, SIGTERM) == -1) {
			perror("Nie udalo sie wyslac SIGTERM do strazaka!");
		}
	}

	while (wait(NULL) > 0);

	printf("\033[31mManager: Odczytuje raport...\033[0m\n");
	read_report();

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
	if (sig == SIGUSR1) 
		fire_alarm = 1;		
}

void read_report() {
	int file = open("reports.txt", O_RDONLY);
	if (file == -1) {
		perror("Blad otwierania pliku.");
		return;
	}

	char buffer[256];
	int bytes_read;

	while ((bytes_read = read(file, buffer, sizeof(buffer) - 1)) > 0) {
		buffer[bytes_read] = '\0';
		printf("\033[32m%s\033[0m", buffer);
	}

	if (bytes_read == -1) {
		perror("Blad odczytu z pliku");
	}

	close(file);
}
