#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include "helper.h"

int main(int argc, char* argv[]) {
	setbuf(stdout, NULL);

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

    	sleep(rand() % 15 + 100);
    	printf("Strazak: POZAR!!!\n");


    	// Sygnaly pozarow do klientow przy stole, menadzera i kasjera
    	if (kill(pid_cashier, SIGUSR1) == -1) {
        	perror("Nie udalo sie wyslac SIGUSR1 do kasjera");
    	}

    	if (kill(pid_manager, SIGUSR1) == -1) {
        	perror("Nie udalo sie wyslac SIGUSR1 do managera");
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
	V(sem_id, SEM_MUTEX_TABLES_DATA);

    	if (shmdt(tables) == -1) 
        	perror("Blad odlaczania pamieci dzielonej");
    	

    return 0;
}

