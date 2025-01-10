#include "LinkedList.h"
#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

void initialize_linked_list(LinkedList* ll, int size) {
	ll->head = NULL;
	ll->size = size;
	ll->current_size = 0;
}

int get_current_size(const LinkedList* ll) {
	return ll->current_size;
}

void add(LinkedList* ll, Client* c) {
	Node* new_node = calloc(1, sizeof(Node));
	new_node->client = c;
	
	if (ll->head == NULL) {
		ll->head = new_node;
	} else {
		Node* temp = ll->head;
		while (temp->next != NULL)
			temp = temp->next;
		temp->next = new_node;
	}
	++ll->current_size;
}

Client* pop(LinkedList* ll, int group_size) {
	if (ll->head == NULL) 
		return NULL;

	Node* temp = ll->head;
	Node* prev = NULL;
	while (temp != NULL && temp->client->group_size != group_size) {
		prev = temp;
		temp = temp->next;
	}

	if (temp == NULL) 
		return NULL;

	Client* to_return = temp->client;
	if (temp == ll->head) 
		ll->head = temp->next;
	else 
		prev->next = temp->next;

	free(temp);
	--ll->current_size;

	return to_return;
}

void display(const LinkedList* ll) {
	Node* temp = ll->head;
	if (temp == NULL) {
		printf("Kolejka jest pusta.\n");
		return;
	}

	int i = 1;
	printf("Kolejka przed lokalem:\n");
	while (temp != NULL) { 
		printf("%d. Grupa klientow (%d), ilosc osob: %d\n", i, temp->client->group_id, temp->client->group_size);
		++i;
		temp = temp->next;
	}
}

void free_linked_list(LinkedList* ll) {
	if (ll->head == NULL)
		return;

	Node* temp = ll->head;
	Node* prev = NULL;

	while (temp != NULL) {
		prev = temp;
		temp = temp->next;
		free(prev);
	}

	ll->current_size = 0;
	ll->head = NULL;
}

