#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <limits.h>
#include <errno.h>
#include "helper.h"

volatile sig_atomic_t fire_alarm = 0;
volatile sig_atomic_t closing_soon = 0;
volatile unsigned long work_time = ULONG_MAX;

void initialize_tables(Table* tables, int start, int end, int capacity);
int find_table(Table* tables, int group_size, int table_count);
void generate_report(int* dishes_count, double total_income, int client_count); 
void signals_handler(int sig);
void tables_status(Table* tables, int table_count);

int main(int argc, char* argv[]) {
        int x1 = atoi(argv[1]);
	int x2 = atoi(argv[2]);
	int x3 = atoi(argv[3]);
	int x4 = atoi(argv[4]);
	int table_count = x1 + x2 + x3 + x4;
		
	setbuf(stdout, NULL);

	// Do obslugi sygnalu
	struct sigaction sa;
	sa.sa_handler = signals_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("Blad sigaction() [kasjer]");
		exit(1);
	}

	if (sigaction(SIGUSR2, &sa, NULL) == -1) {
		perror("Blad sigaction() [kasjer]");
		exit(1);
	}

	// Klucze (semafor, pamiec dzielona, kolejka komunikatow)
        key_t sem_key = ftok(".", SEM_GEN_KEY);
        if (sem_key == -1) {
                perror("Blad generowania klucza w ftok()");
                exit(1);
        }

        key_t shm_key = ftok(".", SHM_GEN_KEY);
        if (shm_key == -1) {
                perror("Blad generowania klucza w ftok()");
                exit(1);
        }

        key_t msg_key = ftok(".", MSG_GEN_KEY);
	if (msg_key == -1) {
		perror("Blad generowania klucza w ftok()");
		exit(1);
	}

	// Tworzenie semafora, pamieci dzielonej, kolejki komunikatow
	int sem_id = create_sem(sem_key);
	int shm_id = create_shm(shm_key, sizeof(Table) * table_count);
	int msg_id = create_msg(msg_key);


	Table* tables = shmat(shm_id, NULL, 0);
	if (tables == (void*)-1) {
		perror("Blad podlaczenia pamieci dzielonej w shmat()");
		exit(1);
	}

        initialize_tables(tables, 0, x1, 1);
        initialize_tables(tables, x1, x1+x2, 2);
        initialize_tables(tables, x1+x2, x1+x2+x3, 3);
        initialize_tables(tables, x1+x2+x3, x1+x2+x3+x4, 4);

	int dishes_count[10] = {0};
	double total_income = 0.0;
	int client_count = 0;
	printf("Kasjer: otwieram kase!\n");

	while(!fire_alarm && ((unsigned long)time(NULL) < work_time)) {
		CashierClientComm msg; 
		msg.table_number = -1;
	
		if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, IPC_NOWAIT) == -1) {
			if (errno == ENOMSG)
				continue;
			else {
				perror("Blad odbierania komunikatu w msgrcv()");
				exit(1);
			}
		}
		
		if (msg.action == TABLE_RESERVATION) {
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			sleep(1);
			int table_num = find_table(tables, msg.group_size, table_count); 
			if (table_num == TABLE_NOT_FOUND) {
				printf("Kasjer: nie znaleziono stolikow dla grupy (%d) %d-osobowej.\n", msg.group_id, msg.group_size);
			} else if (table_num == CLOSING_SOON) {
				printf("Kasjer: Grupo (%d), przykro mi, ale zaraz zamykamy, nie przydzielam stolika.\n", msg.group_id); 
			} else { 
				tables[table_num].current += msg.group_size;
				tables[table_num].group_size = msg.group_size;
				int idx = 0;
				while (idx < 4 && tables[table_num].group_id[idx] != 0)
					idx++;
				tables[table_num].group_id[idx] = msg.group_id;
				printf("Kasjer: stolik nr %d przydzielony dla grupy (%d) %d-osobowej.\n", table_num, msg.group_id, msg.group_size);
			}
			V(sem_id, SEM_MUTEX_TABLES_DATA);

			msg.table_number = table_num;
			msg.mtype = msg.group_id;
			
			if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
				perror("Blad wysylania komunikatu w msgsnd()");
				exit(1);
			}
		} else if (msg.action == ORDER) {
			tables_status(tables, table_count);
			for (int i = 0; i < msg.group_size; ++i) {
				dishes_count[msg.dishes[i]]++;
				total_income += menu[msg.dishes[i]].price;
			}
			client_count += msg.group_size;
		} else if (msg.action == TABLE_EXIT) {
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			int x = 0;
			while (x < 4 && tables[msg.table_number].group_id[x] != msg.group_id)
				x++;

			tables[msg.table_number].group_id[x] = 0;
			tables[msg.table_number].current -= msg.group_size;
			if (tables[msg.table_number].current == 0)
				tables[msg.table_number].group_size = 0;		
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		}
	}

	generate_report(dishes_count, total_income, client_count);
	
	if (fire_alarm == 1) {
		printf("Kasjer: POZAR! Zaraz zamykam kase i szybko generuje raport!\n");
		sleep(2);
		printf("Kasjer: Kasa zamknieta! Uciekam!!!\n");
	} else {
		printf("Kasjer: Zamykam kase i generuje raport!\n");
	}

	remove_msg(msg_id);

	return 0;
}

void initialize_tables(Table* tables, int start, int end, int capacity) {
        for (int i = start; i < end; ++i) {
                for (int j = 0; j < 4; ++j)
                        tables[i].group_id[j] = 0;

                tables[i].capacity = capacity;
                tables[i].group_size = 0;
                tables[i].current = 0;
        }
}

int find_table(Table* tables, int group_size, int table_count) {
	if (closing_soon)
		return CLOSING_SOON;

	for (int i = 0; i < table_count; ++i) {
		if ((tables[i].group_size == 0 || tables[i].group_size == group_size) &&
		   ((tables[i].capacity - tables[i].current - group_size >= 0)))
		   	return i;
	}

	return TABLE_NOT_FOUND;
}

void generate_report(int* dishes_count, double total_income, int client_count) {
	int file = open("reports.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (file == -1) {
		perror("Blad otwarcia pliku!\n");
		exit(1);
	}

	char header_line[] = "-------------RAPORT DZIENNY--------------\n";
       	if (write(file, header_line, strlen(header_line)) == -1) {
		perror("Blad zapisu do pliku!\n");
		close(file);
		exit(1);
	}

	
	char client_count_line[100];
	snprintf(client_count_line, sizeof(client_count_line), "Ilosc klientow: %d\n", client_count);
       	if (write(file, client_count_line, strlen(client_count_line)) == -1) {
		perror("Blad zapisu do pliku\n");
		close(file);
		exit(1);
	}	
	
	char income_line[100];
	snprintf(income_line, sizeof(income_line), "Calkowity przychod: %.2lf zl\n", total_income);
	if (write(file, income_line, strlen(income_line)) == -1) {
		perror("Blad zapisu do pliku\n");
		close(file);
		exit(1);
	}

	char sold_header[] = "Sprzedano:\n";
	if (write(file, sold_header, strlen(sold_header)) == -1) {
		perror("Blad zapisu do pliku!\n");
		close(file);
		exit(1);
	}

	for (int i = 0; i < 10; ++i) {
		char dish_line[100];
		snprintf(dish_line, sizeof(dish_line), "%s: %d\n", menu[i].dish_name, dishes_count[i]);
		if (write(file, dish_line, strlen(dish_line)) == -1) {
			perror("Blad zapisu do pliku!\n");
			close(file);
			exit(1);
		}	
	}

	close(file);
}

void signals_handler(int sig) {
	if (sig == SIGUSR1)
		fire_alarm = 1;
	else if (sig == SIGUSR2) {
		closing_soon = 1;
		work_time = (unsigned long)time(NULL) + 22;
	}
}

void tables_status(Table* tables, int table_count) {
	printf("\n");
	printf("-------------STATUS STOLIKOW--------------\n");
	for (int i = 0; i < table_count; ++i) {
		printf("Stol nr: %d | pojemnosc=%d | obecnie=%d | rozmiar grupy=%d | group_id=[", i, tables[i].capacity, tables[i].current, tables[i].group_size);
		for (int j = 0; j < 4; ++j) {
			if (tables[i].group_id[j] != 0) {
				printf(" %d ", tables[i].group_id[j]);
			}
		}
		printf("]\n");
	}
	printf("------------------------------------------\n\n");
}
