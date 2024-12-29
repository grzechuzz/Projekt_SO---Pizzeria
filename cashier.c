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
#include "helper.h"

volatile sig_atomic_t fire_alarm = 0;

void initialize_tables(Table* tables, int start, int end, int capacity);
int find_table(Table* tables, int group_size, int table_count);
void generate_report(int* dishes_count, double total_income); 
void fire_signal_handler(int sig);

int main(int argc, char* argv[]) {
        int x1 = atoi(argv[1]);
	int x2 = atoi(argv[2]);
	int x3 = atoi(argv[3]);
	int x4 = atoi(argv[4]);
	int table_count = x1 + x2 + x3 + x4;
		
	setbuf(stdout, NULL);
	// Do obslugi sygnalu
	struct sigaction sa;
	sa.sa_handler = fire_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
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

	printf("Kasjer: otwieram kase!\n");

	while(!fire_alarm) {
		CashierClientComm msg; 
		msg.table_number = -1;
	
		if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), 1, 0) == -1) {
			perror("Blad odbierania komunikatu w msgrcv()");
			exit(1);
		}
		
		if (msg.action == TABLE_RESERVATION) {
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
			
			if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
				perror("Blad wysylania komunikatu w msgsnd()");
				exit(1);
			}
		} else if (msg.action == ORDER) {
			for (int i = 0; i < msg.group_size; ++i) {
				dishes_count[msg.dishes[i]]++;
				total_income += menu[msg.dishes[i]].price;
			}
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
	
	if (fire_alarm == 1)
		printf("Kasjer: POZAR! Zamykam natychmiast kase i szybko generuje raport!\n");
	else 
		printf("Kasjer: zamykam kase i generuje raport!\n");

	generate_report(dishes_count, total_income);
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
	for (int i = 0; i < table_count; ++i) {
		if ((tables[i].group_size == 0 || tables[i].group_size == group_size) &&
		   ((tables[i].capacity - tables[i].current - group_size >= 0)))
		   	return i;
	}
	return -1;
}

void generate_report(int* dishes_count, double total_income) {
	time_t now = time(NULL);
	struct tm* local_time = localtime(&now);
	char date[20];
	strftime(date, sizeof(date), "%Y-%m-%d", local_time);
	
	
	printf("----------RAPORT ZA DZIEN %s----------\n", date);
	printf("Calkowity przychod: %lf zl\n", total_income);
	printf("Sprzedano:\n");
	for (int i = 0; i < 10; ++i) 
		printf("%s: %d\n", menu[i].dish_name, dishes_count[i]);
	
	int file = open("reports.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (file == -1) {
		perror("Blad otwarcia pliku!\n");
		exit(1);
	}

	char header_line[100];
	snprintf(header_line, sizeof(header_line), "----------RAPORT ZA DZIEN %s----------\n", date);
       	if (write(file, header_line, strlen(header_line)) == -1) {
		perror("Blad zapisu do pliku!\n");
		close(file);
		exit(1);
	}
	
	char income_line[100];
	snprintf(income_line, sizeof(income_line), "Calkowity przychod: %lf zl", total_income);
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

void fire_signal_handler(int sig) {
	if (sig == SIGUSR1)
		fire_alarm = 1;
	
}
