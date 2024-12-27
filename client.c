#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <unistd.h>
#include "helper.h"

void arg_checker(int argc, char* argv[]);

int main(int argc, char* argv[]) {
	arg_checker(argc, argv);
	
	key_t msg_key = ftok(".", MSG_GEN_KEY);
	if (msg_key == -1) {
		perror("Blad uzyskiwania klucza w ftok()");
		exit(1);
	}

	int msg_id = msgget(msg_key, 0);
	if (msg_id == -1) {
		perror("Blad dolaczania do kolejki komunikatow w msgget()");
		exit(1);
	}

	int n = atoi(argv[1]);

	printf("Grupa klientow (%d) %d-osobowa: zglaszamy zapotrzebowanie na stolik.\n", getpid(), n);
	
	table_reservation msg; // mtype, group_size, group_id, table_number
	msg.mtype = 1;
	msg.group_id = getpid();
	msg.table_number = 0;
	msg.group_size = n;

	if (msgsnd(msg_id, &msg, sizeof(msg) - sizeof(long), 0) == -1) {
		perror("Blad wysylania komunikatu w msgsnd()");
		exit(1);
	} 

	if (msgrcv(msg_id, &msg, sizeof(msg) - sizeof(long), getpid(), 0) == -1) {
		perror("Blad odbierania komunikatu w msgrcv()");
		exit(1);
	}

	if (msg.table_number == -1) 
		printf("Grupa klientow (%d) %d-osobowa: nie chce sie nam czekac, wychodzimy z lokalu.\n", getpid(), n);
	else 
		printf("Grupa klientow (%d) %d-osobowa: Siadamy przy stoliku nr %d", getpid(), n, msg.table_number);


	// tutaj potem bedzie obsluga zamawianego zarcia, w cashier.c to samo ;DDDDDDd

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
