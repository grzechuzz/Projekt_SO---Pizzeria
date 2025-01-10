#ifndef HELPER_H
#define HELPER_H

#include <sys/types.h>

#define SEM_MUTEX_TABLES_DATA 0
#define SEM_GEN_KEY 'A'
#define SHM_GEN_KEY 'B'
#define MSG_GEN_KEY 'C'
#define TABLE_RESERVATION 1
#define ORDER 2
#define TABLE_EXIT 3
#define TABLE_NOT_FOUND -1
#define CLOSING_SOON -2
#define TIME_TO_CLOSE 3
#define MAX_ACTIVE_CLIENTS 400
#define WORK_TIME 30
#define MAX_WAITING_CLIENTS 30

// Struktura stolika, potrzebna do pamieci dzielonej
typedef struct {
	int capacity;
	pid_t group_id[4];
	int group_size;
	int current;
} Table;

// Jedno danie, potrzebne do def. menu
typedef struct {
	const char* dish_name;
	double price;
} Dish;

// Struktura klienta jako grupy n-osobowej (potrzebna do listy oczekujacych + do struktury komunikatow)
typedef struct {
	int group_size;
	pid_t group_id;
} Client;

// Struktura komunikatu (klient <-> kajser)
typedef struct {
	long mtype;
	Client client;
	int table_number;
	int dishes[3];
} CashierClientComm;

// Dla watkow (jako wyodrebniona jedna osoba wew. procesu klienta) 
typedef struct {
	int* orders;
	int size;	
} ClientOrders;

extern Dish menu[10];

int create_sem(key_t key);
int create_shm(key_t key, size_t size);
int create_msg(key_t key);
void remove_sem(int sem_id);
void remove_shm(int shm_id, void* addr);
void remove_msg(int msg_id);
int join_sem(key_t key);
int join_shm(key_t key);
int join_msg(key_t key);
void P(int sem_id, int sem_num);
void V(int sem_id, int sem_num);
void print_single_order(int id);

#endif // HELPER_H
