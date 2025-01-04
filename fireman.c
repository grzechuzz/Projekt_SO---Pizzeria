#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include "helper.h"

void signal_handler(int sig);

int main(int argc, char* argv[]) {
	
	if (argc != 4) {
		perror("Bledna liczba argumentow. Poprawne uzycie ./fireman <pid_kasjera> <pid_managera> <ilosc stolikow>");
	}

	setbuf(stdout, NULL);
	srand(time(NULL));

	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		perror("Blad ustawienia SIGTERM");
		exit(1);
	}

    	pid_t pid_cashier = (pid_t)atoi(argv[1]);
    	pid_t pid_manager = (pid_t)atoi(argv[2]);
    	int table_count = atoi(argv[3]);

    	key_t shm_key = ftok(".", SHM_GEN_KEY);
    	if (shm_key == -1) {
        	perror("Blad uzyskiwania klucza w ftok()");
        	exit(1);
    	}

	key_t sem_key = ftok(".", SEM_GEN_KEY);
	if (sem_key == -1) {
		perror("Blad uzyskiwania klucza w ftok()");
		exit(1);
	}

	int sem_id = join_sem(sem_key);
    	int shm_id = join_shm(shm_key);

    	Table* tables = shmat(shm_id, NULL, 0);
    	if (tables == (void*)-1) {
        	perror("Blad podlaczenia pamieci dzielonej");
        	exit(1);
    	}
    	sleep(40);
    	printf("\033[41mStrazak: POZAR!!!\033[0m\n");


    	// Sygnaly pozarow do klientow przy stole, menadzera i kasjera
    	if (kill(pid_cashier, SIGUSR1) == -1) {
        	perror("Nie udalo sie wyslac SIGUSR1 do kasjera");
    	}
	
	P(sem_id, SEM_MUTEX_TABLES_DATA);
    	for (int i = 0; i < table_count; ++i) {
        	for (int j = 0; j < 4; ++j) {
            		if (tables[i].group_id[j] != 0) {
                		if (kill(tables[i].group_id[j], SIGUSR1) == -1) 
                    			perror("Nie udalo sie wyslac SIGUSR1 do klienta");
            		}
        	}
    	}

	if (kill(pid_manager, SIGUSR1) == -1) {
		perror("Nie udalo sie wyslac SIGUSR1 do managera");
	}
	V(sem_id, SEM_MUTEX_TABLES_DATA);

    	if (shmdt(tables) == -1) 
        	perror("Blad odlaczania pamieci dzielonej");
    	

    return 0;
}


void signal_handler(int sig) {
	if (sig == SIGTERM) {
		printf("\033[31mStrazak: Nie jestem juz potrzebny. Opuszczam lokal!\033[0m\n");
		exit(0);
	}
}
