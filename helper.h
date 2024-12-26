#ifndef HELPER_H
#define HELPER_H

#define SEM_MUTEX_TABLE 0

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
