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
#include "LinkedList.h"

volatile sig_atomic_t fire_alarm = 0;
volatile sig_atomic_t closing_soon = 0;
volatile unsigned long work_time = ULONG_MAX;

void signals_handler(int sig);
void initialize_tables(Table* tables, int start, int end, int capacity);
int find_table(Table* tables, int group_size, int table_count);
void remove_group_from_table(Table* tables, int table_idx, pid_t group_pid, int group_size);
void seat_group(Table* tables, int table_idx, Client* c, int msg_id);
void seat_all_possible_from_queue(Table* tables, LinkedList* waiting_clients, int table_count, int msg_id);
void generate_report(int* dishes_count, double total_income, int client_count); 
void tables_status(Table* tables, int table_count);
void send_closing_soon(LinkedList* waiting_clients, int msg_id);

int main(int argc, char* argv[]) {
	if (argc != 5) {
                fprintf(stderr, "Bledna liczba argumentow. Poprawne uzycie ./cashier <X1> <X2> <X3> <X4>");
                exit(1);
        }

        int x1 = atoi(argv[1]);
	int x2 = atoi(argv[2]);
	int x3 = atoi(argv[3]);
	int x4 = atoi(argv[4]);
	int table_count = x1 + x2 + x3 + x4;
		
	setbuf(stdout, NULL);

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

	// Tworzenie pamieci dzielonej, kolejki komunikatow, dolaczanie do zbioru semaforow
	int sem_id = join_sem(sem_key);
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
	V(sem_id, SEM_INIT_READY); // kasjer zaladowal zasoby, mozna odblokowac managera, by ladowal strazaka

	// Lista jednokierunkowa sluzy jako kolejka przed pizzeria
	LinkedList waiting_clients;
	initialize_linked_list(&waiting_clients, MAX_WAITING_CLIENTS);	

	int dishes_count[10] = {0};
	double total_income = 0.0;
	int client_count = 0;

	printf("\033[32mKasjer: otwieram kase!\033[0m\n");

	while(!fire_alarm && ((unsigned long)time(NULL) < work_time)) {
		// Powiadomienie osob w kolejce, ze zaraz zamykamy (inaczej dowiedzialyby sie dopiero podczas proby wejscia do pizzerii)
		if (!fire_alarm && closing_soon == 1 && get_current_size(&waiting_clients) > 0) 
			send_closing_soon(&waiting_clients, msg_id);

		CashierClientComm msg;

		/* I: Odbior zapytania klienta o stolik:
		 * 1) Sprawdzamy kolejke, przydzielamy stoliki dla jak najwiekszej ilosci grup z kolejki (tyle ile sie da)
		 * 2) Potem szukamy odpowiedniego stolika dla grupy skladajacej zapytanie, find_table moze zwrocic 3 rozne wartosci i na podstawie tego wykonujemy odpowiednia akcje: 
		 * 	a) CLOSING_SOON - zaraz zamykamy, wtedy zapisujemy do pola msg.table_number wartosc CLOSING_SOON, 
		 * 	   klient odbierajac zwrotny komunikat wie, ze juz nie moze wejsc
		 *	b) TABLE_NOT_FOUND - stolik nieznaleziony dla okreslonej grupy, jezeli kolejka nie jest za dluga to grupa w niej staje,
		 *	   w przeciwnym wypadku ustawiamy msg.table_number na TABLE_NOT_FOUND i klienci stwierdzaja, ze nie chce im sie stac w tak dlugiej kolejce
		 *	c) liczba calkowita >= 0 (Numer wolnego stolika) - Wysylamy grupie w polu msg.table_number numer stolika przy ktorym moga siasc 
		 */

		int msgrcv_ret = msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), TABLE_RESERVATION, IPC_NOWAIT);
		if (msgrcv_ret != -1 && !fire_alarm) {
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			if (get_current_size(&waiting_clients) > 0)
				seat_all_possible_from_queue(tables, &waiting_clients, table_count, msg_id);

			int idx = find_table(tables, msg.client.group_size, table_count);
			if (idx == CLOSING_SOON) {
				msg.mtype = msg.client.group_id;
				msg.table_number = CLOSING_SOON;
				printf("\033[32mKasjer: Grupo (%d), przykro mi, ale zaraz zamykamy, nie przydzielam stolika.\033[0m\n", msg.client.group_id);
				if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
					perror("Blad wysylania komunikatu w msgsnd()");
					exit(1);
				}
			} else if (idx == TABLE_NOT_FOUND) {
				if (get_current_size(&waiting_clients) >= MAX_WAITING_CLIENTS) {
					msg.mtype = msg.client.group_id;
					msg.table_number = TABLE_NOT_FOUND;
					printf("\033[32mKasjer: Grupo (%d), czas oczekiwania wyniesie ponad godzine z powodu ogromnej kolejki.\033[0m\n", msg.client.group_id);
					if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
						perror("Blad wysylania komunikatu w msgsnd()");
						exit(1);
					}
				} else {
					add(&waiting_clients, &msg.client);
					display(&waiting_clients); // wyswietlanie kolejki przed lokalem
				}
			} else {
				seat_group(tables, idx, &msg.client, msg_id);
			}
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		} else if (msgrcv_ret == -1) {
			if (errno != ENOMSG && errno != EINTR) {
				perror("Blad odbierania komunikatu w msgrcv()");
				exit(1);
			}
		}
		
		/* II: Odbior wiadomosci od klienta o zlozonym zamowieniu
		 * 1) Klienci wysylaja nam co zamowili: przeliczamy cene i wszystkie zamowione dania i dodajemy je do wyswietlenia w koncowym raporcie
		 * 2) Nie odpowiadamy na ta wiadomosc klientowi, bo "czas od zamowienia do otrzymania pizzy moze byc traktowany jako nieistotny", zatem klient po prostu
		 *    dostaje od razu wybrana przez niego pozycje z menu i z nia siada przy stoliku (dla nas jego wiadomosc jest jedynie informacja co ujac w raporcie) 
		 * 3) Wyswietlamy aktualnie stan klientow siedzacych przy stolikach (opcjonalnie - mozna to zakomentowac)   
		 */

		msgrcv_ret = msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), ORDER, IPC_NOWAIT);	
		if (msgrcv_ret != -1 && !fire_alarm) {
			for (int i = 0; i < msg.client.group_size; ++i) {
				dishes_count[msg.dishes[i]]++;
				total_income += menu[msg.dishes[i]].price;
			}
			client_count += msg.client.group_size;
			P(sem_id, SEM_MUTEX_TABLES_DATA);	
			tables_status(tables, table_count); // wyswietlanie stanu stolikow
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		} else if (msgrcv_ret == -1) {
			if (errno != ENOMSG && errno != EINTR) {
				perror("Blad odbierania komunkatu w msgrcv()");
				exit(1);
			}
			
		}

		/* III: Odbior wiadomosci klienta o opuszczeniu stolika
		 * 1) Klient po zjedzeniu wysyla do nas informacje, ze opuszcza stolik, uzywamy tej informacji do zaktualizowania stanu stolikow
		 * 2) Od razu po tej informacji probujemy przydzielic stoliki ludziom z kolejki, aby ja troche rozladowac
		 */
		
		msgrcv_ret = msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), TABLE_EXIT, IPC_NOWAIT);
		if (msgrcv_ret != -1 && !fire_alarm) {
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			remove_group_from_table(tables, msg.table_number, msg.client.group_id, msg.client.group_size);
			if (get_current_size(&waiting_clients) > 0)
				seat_all_possible_from_queue(tables, &waiting_clients, table_count, msg_id);
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		} else if (msgrcv_ret == -1) {
			if (errno != ENOMSG && errno != EINTR) {
				perror("Blad odbierania komunikatu w msgrcv()");
				exit(1);
			}
		}
	}

	if (!fire_alarm) 
		 printf("\033[32mKasjer: Jezeli ktos jeszcze je, to daje mu dokonczyc...\033[0m\n");	
	
	// Przy pozarze: upewniam sie, ze wszyscy wyjda
	// Gdy nie ma pozaru: daje dokonczyc jedzenie pozostalym
	// Nie bedzie nieskonczonej petli w przypadku pozaru, bo strazak ustawia stoliki na 0
	while (1) {
		int check_tables = 1;
		for (int i = 0; i < table_count; ++i) {
			if (tables[i].current != 0) {
				check_tables = 0;
				break;
			}
		}
			
		if (check_tables)
			break;
		
		if (!fire_alarm) {	
			CashierClientComm msg;
			if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), TABLE_EXIT, IPC_NOWAIT) == -1)  {
				if (errno == ENOMSG || errno == EINTR) 
					continue;
				else {
				perror("Blad odbierania komunikatu w msgrcv()");
				exit(1);
				}
			}
			P(sem_id, SEM_MUTEX_TABLES_DATA);
			remove_group_from_table(tables, msg.table_number, msg.client.group_id, msg.client.group_size);
			V(sem_id, SEM_MUTEX_TABLES_DATA);
		}
	}
		
	generate_report(dishes_count, total_income, client_count);
	//sleep(1);
	printf("\033[32mKasjer: Zamykam kase!\033[0m\n");

	remove_msg(msg_id);
	remove_shm(shm_id, tables);
	remove_sem(sem_id);

	return 0;
}

/**
 * initialize_tables - Inicjalizuje stoliki w pizzerii.
 * @tables: Tablica struktur stolikow [pamiec dzielona].
 * @start: Indeks poczatkowy dla inicjalizacji stolikow.
 * @end: Indeks koncowy dla inicjalizacji stolikow.
 * @capacity: Liczba miejsc przy kazdym stoliku w podanym zakresie.
 *
 * Ustawia stoliki z odpowiednia pojemnoscia w zadanym zakresie,
 * oznaczajac je jako puste.
 */

void initialize_tables(Table* tables, int start, int end, int capacity) {
        for (int i = start; i < end; ++i) {
                for (int j = 0; j < 4; ++j)
                        tables[i].group_id[j] = 0;

                tables[i].capacity = capacity;
                tables[i].group_size = 0;
                tables[i].current = 0;
        }
}

/**
 * find_table - Znajduje odpowiedni stolik dla grupy klientow.
 * @tables: Tablica struktur stolikow.
 * @group_size: Liczba osob w grupie klientow.
 * @table_count: Liczba wszystkich stolikow.
 *
 * Szuka pierwszego dostepnego stolika, ktory spelnia wymagania grupy
 * co do liczby miejsc. Jezeli lokal zaraz sie zamknie, zwraca CLOSING_SOON.
 * Jezeli brak stolika, zwraca TABLE_NOT_FOUND.
 */

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

/**
 * remove_group_from_table - Usuwa grupe klientow ze stolika.
 * @tables: Tablica struktur stolikow.
 * @table_idx: Indeks stolika.
 * @group_id: Identyfikator grupy klientow.
 * @group_size: Rozmiar grupy klientow.
 *
 * Zwolnia miejsca zajmowane przez grupe w wybranym stoliku
 * oraz resetuje informacje o grupie, jesli stolik staje sie pusty.
 */

void remove_group_from_table(Table* tables, int table_idx, pid_t group_id, int group_size) {
	int i = 0;
	while (i < 4 && tables[table_idx].group_id[i] != group_id)
		++i;

	tables[table_idx].group_id[i] = 0;
	tables[table_idx].current -= group_size;

	if (tables[table_idx].current == 0)
		tables[table_idx].group_size = 0;
}

/**
 * seat_group - Umieszcza grupe klientow przy stoliku.
 * @tables: Tablica struktur stolikow.
 * @table_idx: Indeks wybranego stolika.
 * @c: Struktura reprezentujaca grupe klientow.
 * @msg_id: Identyfikator kolejki komunikatow.
 *
 * Rejestruje grupe klientow przy wybranym stoliku i wysyla
 * potwierdzenie do grupy z informacjami o stoliku.
 */

void seat_group(Table* tables, int table_idx, Client* c, int msg_id) {
	if (tables[table_idx].current == 0) 
		tables[table_idx].group_size = c->group_size; 

	tables[table_idx].current += c->group_size;

	int i = 0;
	while (i < 4 && tables[table_idx].group_id[i] != 0) 
		++i;

	tables[table_idx].group_id[i] = c->group_id;

	CashierClientComm msg;
	msg.mtype = c->group_id;
	msg.client = *c;
	msg.table_number = table_idx;

	printf("\033[32mKasjer: stolik nr %d przydzielony dla grupy (%d) %d-osobowej.\033[0m\n", table_idx, c->group_id, c->group_size);
	if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
		perror("Blad wysylania komunikatu w msgsnd()");
		exit(1);
	}
}

/**
 * seat_all_possible_from_queue - Umieszcza przy stolikach wszystkie pasujace grupy z kolejki.
 * @tables: Tablica struktur stolikow.
 * @waiting_clients: Lista jednokierunkowa reprezentujaca kolejke klientow.
 * @table_count: Liczba wszystkich stolikow.
 * @msg_id: Identyfikator kolejki komunikatow.
 *
 * Iteruje przez kolejke klientow i przypisuje stoliki grupom,
 * w zaleznosci od ich rozmiaru oraz dostepnych miejsc przy stolikach.
 * Jezeli grupa zostanie usadzona, jest usuwana z kolejki.
 */

void seat_all_possible_from_queue(Table* tables, LinkedList* waiting_clients, int table_count, int msg_id) {
	int changed = 1;
	while (changed) {
		changed = 0;
		for (int i = 0; i < table_count; ++i) {
			int free_seats = tables[i].capacity - tables[i].current;
			if (free_seats <= 0)
				continue;

			int current_groups_size = tables[i].group_size;
			if (free_seats < current_groups_size)
				continue;
			
			Client* c = pop_suitable(waiting_clients, current_groups_size, free_seats);
		       	if (c != NULL) {
				seat_group(tables, i, c, msg_id);
				changed = 1;
				free(c);			
			}	
		}
	}
}

// zapis raportu do pliku

void generate_report(int* dishes_count, double total_income, int client_count) {
	int file = open("reports.txt", O_WRONLY | O_CREAT | O_TRUNC, 0600);
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

/**
 * signals_handler - Obsluguje sygnaly SIGUSR1 i SIGUSR2.
 * @sig: Numer sygnalu.
 *
 * Dla SIGUSR1 ustawia flage fire_alarm, co oznacza ewakuacje.
 * Dla SIGUSR2 ustawia flage closing_soon oraz dostosowuje czas
 * pozostaly do zamkniecia pizzerii.
 */

void signals_handler(int sig) {
	if (sig == SIGUSR1) {
		fire_alarm = 1;
		printf("\033[32mKasjer: POZAR! Sprawdzam czy wszyscy klienci opuscili lokal...\033[0m\n");	
	} else if (sig == SIGUSR2) {
		closing_soon = 1;
		work_time = (unsigned long)time(NULL) + TIME_TO_CLOSE;
	}
}

// wyswietlenie aktualnego stanu stolikow

void tables_status(Table* tables, int table_count) {
	printf("\n");
	printf("\033[32m-------------STATUS STOLIKOW--------------\033[0m\n");
	for (int i = 0; i < table_count; ++i) {
		printf("\033[32mStol nr: %d | pojemnosc=%d | obecnie=%d | rozmiar grupy=%d | group_id=[\033[0m", i, tables[i].capacity, tables[i].current, tables[i].group_size);
		for (int j = 0; j < 4; ++j) {
			if (tables[i].group_id[j] != 0) {
				printf(" %d ", tables[i].group_id[j]);
			}
		}
		printf("\033[32m]\n\033[0m");
	}
	printf("\033[32m------------------------------------------\033[0m\n\n");
}

/**
 * send_closing_soon - Informuje grupy w kolejce o zamknieciu lokalu.
 * @waiting_clients: Lista jednokierunkowa reprezentujaca kolejke klientow.
 * @msg_id: Identyfikator kolejki komunikatow.
 *
 * Wysyla wiadomosc do wszystkich oczekujacych grup, informujac ich,
 * ze pizzeria zaraz zostanie zamknieta i nie ma sensu czekac.
 */

void send_closing_soon(LinkedList* waiting_clients, int msg_id) {
	Node* temp = waiting_clients->head;

	while (temp != NULL) {
		Client* c = temp->client;
		CashierClientComm msg;

		msg.mtype = c->group_id;
		msg.client = *c;
		msg.table_number = CLOSING_SOON;
		
		printf("\033[32mKasjer: Grupo (%d), przykro mi, ale zaraz zamykamy, nie przydziele wam stolika.\033[0m\n", msg.client.group_id);

		if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
			perror("Blad wysylania komunikatu w msgsnd()");
			exit(1);
		}

		temp = temp->next;
	}
	waiting_clients->current_size = 0;
}

