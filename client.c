#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "helper.h"

void arg_checker(int argc, char* argv[]);
void* single_person_order(void* orders);
void fire_signal_handler(int sig);
pthread_mutex_t mutex;

int main(int argc, char* argv[]) {
	arg_checker(argc, argv);
	srand(time(NULL));
	setbuf(stdout, NULL);

	// Obsluga sygnalu pozaru dla klientow
	struct sigaction sa;
	sa.sa_handler = fire_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("Blad sigaction() [sygnal klient]");
		exit(1);
	}

	// Dolaczenie do kolejki komunikatow (wymiana komunikatow kajser<->klient)
	key_t msg_key = ftok(".", MSG_GEN_KEY);
	if (msg_key == -1) {
		perror("Blad uzyskiwania klucza w ftok()");
		exit(1);
	}

	int msg_id = join_msg(msg_key);

	int n = atoi(argv[1]);
	printf("Grupa klientow (%d) %d-osobowa: zglaszamy zapotrzebowanie na stolik.\n", getpid(), n);

	CashierClientComm msg;
	msg.mtype = 1;
	msg.action = TABLE_RESERVATION;
	msg.group_size = n;
	msg.group_id = getpid();
	msg.table_number = -1;

	// Zgloszenie zapotrzebowania na stolik
	if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
		perror("Blad wysylania komunikatu w msgsnd()");
		exit(1);
	} 

	// Odbior komunikatu przez klientow czy jest wolny stolik czy nie
	if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), getpid(), 0) == -1) {
		perror("Blad odbierania komunikatu w msgrcv()");
		exit(1);
	}

	if (msg.table_number == TABLE_NOT_FOUND) {
		printf("Grupa klientow (%d) %d-osobowa: Nie chce sie nam czekac, wychodzimy z lokalu.\n", getpid(), n);
		exit(0);
	} else if (msg.table_number == CLOSING_SOON) {
		printf("Grupa kilentow (%d) %d-osobowa: Szkoda, sprobujemy ponownie jutro!\n", getpid(), n);
		exit(0);
	}

	// Skladanie zamowienia (siadziemy przy stole dopiero, gdy zamowimy :D)
	for (int i = 0; i < 3; ++i) 
		msg.dishes[i] = -1;
	
	
	if (pthread_mutex_init(&mutex, NULL) != 0) {
		perror("Blad w pthread_mutex_init()");
		exit(1);
	}

	int* orders = malloc(n * sizeof(int));
	for (int i = 0; i < n; ++i)
		orders[i] = -1;

	pthread_t* tids = calloc(n, sizeof(pthread_t));

	ClientOrders co_d;
	co_d.orders = orders;
	co_d.size = n;
	ClientOrders* co = &co_d;

	for (int i = 0; i < n; ++i) {
		if (pthread_create(&tids[i], NULL, single_person_order, (void*)co) == -1) {
			perror("Blad podczas tworzenia watku w pthread_create()");
			exit(1);
		}	
	}

	for (int i = 0; i < n; ++i) {
		if (pthread_join(tids[i], NULL) == -1) {
			perror("Blad pthread_join()");
			exit(1);
		}
	}
	
	double total_price = 0;
	msg.mtype = 1;
	msg.action = ORDER;
	for (int i = 0; i < n; ++i) {
		msg.dishes[i] = orders[i];
		total_price += menu[orders[i]].price;
	}

	if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
		perror("Blad wysylania komunikatu w msgsnd()");
		exit(1);
	}

	printf("Grupa klientow (%d) %d-osobowa: Skladamy zamowienie na laczna kwote %.2lf zl. Siadamy z nim przy stoliku nr %d.\n", getpid(), n, total_price, msg.table_number);

	// Jedzenie
	sleep(15);

	// Opuszczanie stolika 
	msg.mtype = 1;
	msg.action = TABLE_EXIT;
	if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
		perror("Blad wysylania komunikatu w msgsnd()");
		exit(1);	
	}
	printf("Grupa kilentow (%d) %d-osobowa: Opuszczamy stolik nr %d.\n", getpid(), n, msg.table_number);


	pthread_mutex_destroy(&mutex);
	free(orders);
	free(tids);

	return 0;
}

void arg_checker(int argc, char* argv[]) {
	if (argc != 2) {
		fprintf(stderr, "Bledna liczba argumentow. Poprawne uzycie ./client <ilosc_w_grupie>\n");
		exit(1);
	}

	int n = atoi(argv[1]);

	if (!(n >= 1 && n <= 3)) {
		fprintf(stderr, "Kazda grupa klientow powinna miec 1-3 osoby\n");
		exit(1);
	}
}

void* single_person_order(void* orders) {
	ClientOrders* co = (ClientOrders*)orders;

	pthread_mutex_lock(&mutex);
	int x = 0;
	while (x < co->size && co->orders[x] != -1)
		++x;
	co->orders[x] = rand() % 10;
	printf("Grupa klientow (%d): Osoba %lu ", getpid(), pthread_self());
	print_single_order(co->orders[x]);
	pthread_mutex_unlock(&mutex);
	
	pthread_exit(NULL);
}

void fire_signal_handler(int sig) {
	if (sig == SIGUSR1) {
		printf("Grupa klientow (%d): Pali sie! UCIEKAMY!!111\n", getpid());
		exit(0);
	}
}
