#ifndef HELPER_H
#define HELPER_H

#define SEM_MUTEX_TABLES_DATA 0
#define SEM_GEN_KEY 'A'
#define SHM_GEN_KEY 'B'
#define MSG_GEN_KEY 'C'

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

extern dish menu[10];

void P(int sem_id, int sem_num);
void V(int sem_id, int sem_num);

#endif // HELPER_H
