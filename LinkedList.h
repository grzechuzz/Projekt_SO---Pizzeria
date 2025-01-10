#ifndef LINKEDLIST_H
#define LINKEDLIST_H
#include "helper.h"

typedef struct Node {
	Client* client;
	struct Node* next;	
} Node;

typedef struct LinkedList {
	Node* head;
	int size;
	int current_size;
} LinkedList;

void initialize_linked_list(LinkedList* ll, int size);
int get_current_size(const LinkedList* ll);
void add(LinkedList* ll, Client* c);
Client* pop(LinkedList* ll, int group_size);
void display(const LinkedList* ll);
void free_linked_list(LinkedList* ll);

#endif // LINKEDLIST_H

