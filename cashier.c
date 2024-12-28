#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "helper.h"

void initialize_tables(table* tables, int start, int end, int capacity);
int find_table(table* tables, int group_size, int table_count);

int main(int argc, char* argv[]) {
        int x1 = atoi(argv[1]);
	int x2 = atoi(argv[2]);
	int x3 = atoi(argv[3]);
	int x4 = atoi(argv[4]);
	int table_count = x1 + x2 + x3 + x4;

        key_t sem_key = ftok(".", SEM_GEN_KEY);
        if (sem_key == -1) {
                perror("Blad generowania klucza w ftok()");
                exit(1);
        }

        int sem_id = semget(sem_key, 1, IPC_CREAT|0644);
        if (sem_id == -1) {
                perror("Blad tworzenia semaforow w semget()");
                exit(1);
        }

        if (semctl(sem_id, SEM_MUTEX_TABLES_DATA, SETVAL, 1) == -1) {
                perror("Blad inicjalizacji semafora w semctl()");
                exit(1);
        }

        key_t shm_key = ftok(".", SHM_GEN_KEY);
        if (shm_key == -1) {
                perror("Blad generowania klucza w ftok()");
                exit(1);
        }

        int shm_id = shmget(shm_key, sizeof(table) * table_count, IPC_CREAT|0644);
        if (shm_id == -1) {
                perror("Blad tworzenia pamieci dzielonej w shmget()");
                exit(1);
        }

        table* tables = shmat(shm_id, NULL, 0);
        if (tables == (void*)-1) {
                perror("Blad podlaczenia pamieci dzielonej w shmat()");
                exit(1);
        }

	key_t msg_key = ftok(".", MSG_GEN_KEY);
	if (msg_key == -1) {
		perror("Blad generowania klucza w ftok()");
		exit(1);
	}


	int msg_id = msgget(msg_key, IPC_CREAT|0644);
	if (msg_id == -1) {
		perror("Blad tworzenia kolejki komunikatow w msgget()");
		exit(1);
	}	

        initialize_tables(tables, 0, x1, 1);
        initialize_tables(tables, x1, x1+x2, 2);
        initialize_tables(tables, x1+x2, x1+x2+x3, 3);
        initialize_tables(tables, x1+x2+x3, x1+x2+x3+x4, 4);

	printf("Kasjer: otwieram kase!\n");
	
	while(1) {
		cashier_client_comm msg; // mtype, action, group_size, group_id, table_number, dishes, total_price
		msg.table_number = -1;
	
		if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) {
			perror("Blad odbierania komunikatu w msgrcv()");
			exit(1);
		}
		
		if (msg.action == TABLE_RESERVATION) {
			// Blokada sekcji krytycznej (manager tez ma dostep do tables)
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			int table_num = find_table(tables, msg.group_size, table_count);
			if (table_num == -1) {
				printf("Kasjer: nie znaleziono stolikow dla grupy (%d) %d-osobowej.\n", msg.group_id, msg.group_size);
			} else { 
				tables[table_num].current += msg.group_size;
				tables[table_num].group_size = msg.group_size;
				int group = 0;
				while (group < 4 && tables[table_num].group_id[group] != 0)
					group++;
				tables[table_num].group_id[group] = msg.group_id;
				printf("Kasjer: stolik nr %d przydzielony dla grupy (%d) %d-osobowej.\n", table_num, msg.group_id, msg.group_size);
			}
			V(sem_id, SEM_MUTEX_TABLES_DATA);

			msg.table_number = table_num;
			msg.mtype = msg.group_id;
			//  Odpowiedz czy jest stolik, klient sie skapnie po table_number czy jest czy nie
			if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
				perror("Blad wysylania komunikatu w msgsnd()");
				exit(1);
			}
		} else if (msg.action == ORDER) {
			//TODO 	

		printf("Kasjer: Grupa (%d) %d-osobowa, zrobila zamowienie za %lf zlotych. Siedza przy stoliku nr %d\n",
		       msg.group_id, msg.group_size, msg.total_price, msg.table_number);
		} else if (msg.action == TABLE_EXIT) {
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			int x = 0;
			while (x < 4 && != tables[msg.table_number].group_id[x])
				x++;

			tables[msg.table_number].group_id[x] = 0;
			tables[msg.table_number].current -= msg.group_size;
			if (tables[msg.table_number].current == 0)
				tables[msg.table_number].group_size = 0;		
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		}

	}

	msgctl(msg_id, IPC_RMID, NULL);
	semctl(sem_id, 0, IPC_RMID);
	shmdt(tables);
	shmctl(shm_id, IPC_RMID, NULL);

	return 0;
}

void initialize_tables(table* tables, int start, int end, int capacity) {
        for (int i = start; i < end; ++i) {
                for (int j = 0; j < 4; ++j)
                        tables[i].group_id[j] = 0;

                tables[i].capacity = capacity;
                tables[i].group_size = 0;
                tables[i].current = 0;
        }
}

int find_table(table* tables, int group_size, int table_count) {
	for (int i = 0; i < table_count; ++i) {
		if ((tables[i].group_size == 0 || tables[i].group_size == group_size) &&
		   ((tables[i].capacity - tables[i].current - group_size >= 0)))
		   	return i;
	}
	return -1;
}

