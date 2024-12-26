#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include "helper.h"

dish menu[10] = {	
	{"Pizza Margherita", 33.99},
	{"Pizza Capricciosa", 42.99},
	{"Pizza Neapolitanska", 44.99},
	{"Pizza Pepperoni", 37.99},
	{"Pizza Salami", 42.99},
	{"Pizza Hawajska", 44.99},
	{"Pizza Diavola", 46.99},
	{"Pizza Owoce Morza", 49.99},
	{"Pizza Wiejska", 44.99},
	{"Pizza Vegie Supreme", 44.99}
};

void P(int sem_id, int sem_num) {
	struct sembuf sbuf;
	sbuf.sem_num = sem_num;
	sbuf.sem_op = -1;
	sbuf.sem_flg = 0;

	if (semop(sem_id, &sbuf, 1) == -1) {
		perror("Operacja P nie powiodla sie!");
		exit(1);
	}
}

void V(int sem_id, int sem_num) {
	struct sembuf sbuf;
	sbuf.sem_num = sem_num;
	sbuf.sem_op = 1;
	sbuf.sem_flg = 0;

	if (semop(sem_id, &sbuf, 1) == -1) {
		perror("Operacja V nie powiodla sie!");
		exit(1);
	}

}
