#ifndef HELPER_H
#define HELPER_H

#define SEM_MUTEX_TABLES_DATA 0

#define SEM_GEN_KEY 'A'
#define SHM_GEN_KEY 'B'
#define MSG_GEN_KEY 'C'

#define TABLE_RESERVATION 1
#define ORDER 2
#define TABLE_EXIT 3

typedef struct {
	int capacity;
	int group_id[4];
	int group_size;
	int current;
} table;

typedef struct {
	const char* dish_name;
	double price;
} dish;

typedef struct {
	long mtype;
	int action;
	int group_size;
	int group_id;
	int table_number;
	int dishes[3];
	double total_price;
} cashier_client_comm;


extern dish menu[10];

void P(int sem_id, int sem_num);
void V(int sem_id, int sem_num);

#endif // HELPER_H
