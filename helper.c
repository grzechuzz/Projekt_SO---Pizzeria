#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include "helper.h"

Dish menu[10] = {	
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


int create_sem(key_t key) {
	int sem_id = semget(key, 1, IPC_CREAT|0644);
	if (sem_id == -1) {
		perror("blad w semget()");
		exit(1);
	}

	if (semctl(sem_id, SEM_MUTEX_TABLES_DATA, SETVAL, 1) == -1) {
		perror("blad w semctl() [inicjalizacja semaforow]");
		exit(1);
	}
	
	return sem_id;
}

int create_shm(key_t key, size_t size) {
	int shm_id = shmget(key, size, IPC_CREAT|0644);
	if (shm_id == -1) {
		perror("blad w shmget()");
	}

	return shm_id;
}

int create_msg(key_t key) {
	int msg_id = msgget(key, IPC_CREAT|0644);
	if (msg_id == -1) {
		perror("blad w msgget()");
		exit(1);
	}

	return msg_id;
}

void remove_sem(int sem_id) {
	if (semctl(sem_id, 0, IPC_RMID) == -1) {
		perror("blad w semctl() [usuwanie semaforow]");
	}
}

void remove_shm(int shm_id, void* addr) {
	if (shmdt(addr) == -1) {
		perror("blad w shmdt()");
	}

	if (shmctl(shm_id, IPC_RMID, NULL) == -1) {
		perror("blad w shmctl() [usuwanie pamieci dzielonej]");
	}
}

void remove_msg(int msg_id) {
	if (msgctl(msg_id, IPC_RMID, NULL) == -1)
		perror("blad w msgctl() [usuwanie kolejki komunikatow]");
}

int join_sem(key_t key) {
	int sem_id = semget(key, 0, 0);
	if (sem_id == -1) {
		perror("blad w semget() [dolaczanie do zbioru semaforow]");
		exit(1);
	}

	return sem_id;
}

int join_shm(key_t key) {
	int shm_id = shmget(key, 0, 0);
	if (shm_id == -1) {
		perror("blad w shmget() [dolaczanie do pamieci dzielonej]");
		exit(1);
	}

	return shm_id;
}

int join_msg(key_t key) {
	int msg_id = msgget(key, 0);
	if (msg_id == -1) {
		perror("blad w msgget() [dolaczanie do kolejki komunikatow]");
		exit(1);
	}

	return msg_id;
}

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

void print_single_order(int id) {
	printf("wybiera: %s w cenie %.2lf!\n", menu[id].dish_name, menu[id].price);
}
