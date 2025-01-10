#ifndef LINKEDLIST_H
#define LINKEDLIST_H
#include "helper.h"

typedef struct {
	Client client;
	Node* next;	
} Node;

typedef struct {
	Node* head;
	int size;
	int current_size;
} LinkedList;

Node* initialize_node(Client c);
void initialize_linked_list(LinkedList* ll);
int get_current_size(const LinkedList* ll);
void add(LinkedList* ll, Client c);
Client remove(const LinkedList* ll, int group_size);
void display(LinkedList* ll);

#endif // LINKEDLIST_H

