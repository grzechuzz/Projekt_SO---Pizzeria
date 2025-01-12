#include "LinkedList.h"
#include "helper.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

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
	Client* copy = calloc(1, sizeof(Client));
	memcpy(copy, c, sizeof(Client));

	new_node->client = copy;
	new_node->next = NULL;
	
	if (ll->head == NULL) {
		ll->head = new_node;
	} else {
		Node* temp = ll->head;
		while (temp->next != NULL)
			temp = temp->next;
		temp->next = new_node;
	}
	ll->current_size++;
}

Client* pop_suitable(LinkedList* ll, int needed_group_size, int available_seats) {
	if (ll->head == NULL) 
		return NULL;
	
	Node* prev = NULL;
	Node* temp = ll->head;

	while (temp != NULL) {
		Client* c = temp->client;
		int fits = 0;

		if (needed_group_size == 0) {
			if (c->group_size <= available_seats)
				fits = 1;
		} else {
			if (c->group_size == needed_group_size && c->group_size <= available_seats)
				fits = 1;
		}

		if (fits) {
			if (prev == NULL)
				ll->head = temp->next;
			else 
				prev->next = temp->next;

			ll->current_size--;
			Client* to_return = c;
			free(temp);
			return to_return;
		}

		prev = temp;
		temp = temp->next;
	}

	return NULL;
}

void display(const LinkedList* ll) {
	Node* temp = ll->head;
	if (temp == NULL) {
		printf("Kolejka jest pusta.\n");
		return;
	}

	int i = 1;
	printf("\n");
	printf("\033[1;31mKolejka przed lokalem:\033[0m\n");
	while (temp != NULL) { 
		printf("\033[1;31m%d. Grupa klientow (%d), ilosc osob: %d\033[0m\n", i, temp->client->group_id, temp->client->group_size);
		++i;
		temp = temp->next;
	}
	printf("\n");
}

void free_linked_list(LinkedList* ll) {
	Node* temp = ll->head;
	while (temp != NULL) {
		Node* to_free = temp;
		temp = temp->next;

		if (to_free->client)
			free(to_free->client);

		free(to_free);
	}
	ll->head = NULL;
	ll->current_size = 0;
}

