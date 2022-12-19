/* Eli Wilson CS360
 * 12/15/2022
 * Chat server, can run multiple chat rooms that 
 * can be connected to using "nc host port"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "jrb.h"
#include "dllist.h"
#include "sockettome.h"

typedef struct client {
	FILE *flin;
	FILE *flout;
	char *name;
	Dllist listNode;
} Client;

typedef struct roomInfo {
	char *name;
	Dllist clients;
	Dllist msgs;
	pthread_mutex_t *lock;
	pthread_cond_t *recd;
} RoomInfo;

JRB rooms;

// Gets text from file, returns as string
char * getText(FILE *fl1);

// prints prompt in fl then gets and returns string
char * prompt(char *prompt, FILE *fl, FILE *fl2);

// Waits for client to send message then sends it to 
// all clients in same room
void *roomThread(void *v){
	RoomInfo *info = (RoomInfo *)v;
	Client *client;
	Dllist tmp, tmp2;

	pthread_mutex_lock(info->lock);
	while(1){
		pthread_cond_wait(info->recd, info->lock); // cond is set by client
		
		dll_traverse(tmp, info->msgs){
			if(tmp->val.s != NULL){

				// traverses list of clients in room
				dll_traverse(tmp2, info->clients){
					client = (Client *)tmp2->val.v;
					fputs(tmp->val.s, client->flout);
					fflush(client->flout);
				}
				free(jval_s(dll_val(tmp)));
			}
		}

		free_dllist(info->msgs);
		info->msgs = new_dllist();

	}
	pthread_mutex_unlock(info->lock);
	return NULL;
}

// Called when client disconnects
// Removes client from room, sends leaving message, closes/frees
void leave(Client *client, RoomInfo *info){
	char *s = malloc(200);

	sprintf(s, "%s has left\n", client->name);

	dll_delete_node(client->listNode);
	
	dll_append(info->msgs, new_jval_s(strdup(s)));

	pthread_cond_signal(info->recd);

	fclose(client->flout);
	fclose(client->flin);
	free(client->name);
	free(client);
	free(s);
}

// Prompts for username and room
// Repeatedly gets messages and enters room thread
void *clientThread(void *v){
	Client *client = (Client *)v;
	JRB iter;
	Dllist tmp;
	char *name, *room;
	char *m, *message;
	RoomInfo *info;

	// Prints out rooms and connected users
	fputs("Chat Rooms:\n\n", client->flout);
	jrb_traverse(iter, rooms){
		fputs(jval_s(iter->key), client->flout);
		fputs(":", client->flout);
		info = (RoomInfo *)(iter->val.v);
		pthread_mutex_lock(info->lock);
		dll_traverse(tmp, info->clients){
			fputs(" ", client->flout);
			fputs(((Client *)(tmp->val.v))->name, client->flout);
		}
		pthread_mutex_unlock(info->lock);
		fputs("\n", client->flout);
	}

	// User enters name
	name = prompt("\nEnter your chat name (no spaces):\n", client->flout, client->flin);
	if(name == NULL){
		fclose(client->flout);
		fclose(client->flin);
		pthread_exit(NULL);
	}
	client->name = name;

	// User enters room
	room = prompt("Enter chat room:\n", client->flout, client->flin);
	if(room == NULL){
		fclose(client->flout);
		fclose(client->flin);
		pthread_exit(NULL);
	}
	iter = jrb_find_str(rooms, room);
	if(iter == NULL){
		printf("invalid room recd\n");
		pthread_exit(NULL);
	}
	info = (RoomInfo *)(iter->val.v);

	// Adds user to room, Prints "User has joined" message
	m = malloc(100);
	sprintf(m, "%s has joined\n", client->name);

	pthread_mutex_lock(info->lock);
	dll_append(info->clients, new_jval_v((void *)client));
	client->listNode = dll_last(info->clients);

	dll_append(info->msgs, new_jval_s(strdup(m)));
	pthread_cond_signal(info->recd);
	free(m);
	pthread_mutex_unlock(info->lock);

	// Message receiving
	while(1){
		m = getText(client->flin);
		if(m == NULL){

			pthread_mutex_lock(info->lock);
			leave(client, info);
			pthread_mutex_unlock(info->lock);
			pthread_exit(NULL);
		}

		message = malloc(200);
		sprintf(message, "%s: %s\n", name, m);
		free(m);

		// Adds message to list, unblocks room thread
		pthread_mutex_lock(info->lock);
		dll_append(info->msgs, new_jval_s(strdup(message)));
		pthread_cond_signal(info->recd);
		pthread_mutex_unlock(info->lock);

		free(message);
	}
	pthread_exit(NULL);

}

int main(int argc, char** argv) {
	//char *hn;
	int port, sock, fd;
	pthread_t tcb;
	Client *client;
	rooms = make_jrb();
	FILE *fl, *fl2;
	RoomInfo *info;

	if (argc < 3) {
	    fprintf(stderr, "usage: serve1 port chatrooms\n");
	    exit(1);
	}

	port = atoi(argv[1]);
	if (port < 5000) {
		fprintf(stderr, "usage: serve1 hostname port\n");
		fprintf(stderr, "       port must be > 5000\n");
		exit(1);
	}

	// Creates room threads and adds rooms to tree
	for(int i = 2; i < argc; i++){

		info = malloc(sizeof(RoomInfo));
		info->name = argv[i];
		info->msgs = new_dllist();
		info->clients = new_dllist();
		info->lock = malloc(sizeof(pthread_mutex_t));
		info->recd = malloc(sizeof(pthread_cond_t));
		pthread_mutex_init(info->lock, NULL);
		pthread_cond_init(info->recd, NULL);

		jrb_insert_str(rooms, strdup(argv[i]), new_jval_v(info));
		if (pthread_create(&tcb, NULL, roomThread, (void *)info) != 0) {
			perror("pthread_create");
			exit(1);
		}
	}
	
	/*hn = malloc(200);
	gethostname(hn, 200);
	printf("host: %s\n", hn);*/
	sock = serve_socket(port);

	// Waits for clients to connect, creates client thread
	while(1){
		fd = accept_connection(sock);
		fl = fdopen(fd, "r"); 
		fl2 = fdopen(fd, "w"); 

		client = malloc(sizeof(Client));
		client->flin = fl;
		client->flout = fl2;
		if (pthread_create(&tcb, NULL, clientThread, (void *)client) != 0) {
			perror("pthread_create");
			exit(1);
		}

	}

}

// Gets text from file
char * getText(FILE *fl1){
	char *s = malloc(200);
		if(fgets(s, 100, fl1) == NULL){
			return NULL;
		}else{
			s[strlen(s)-1] = '\0'; // ***potential bug
			return s;
		}	
}

// Sends prompt then gets text
char * prompt(char *str, FILE *fl, FILE *fl2){
	char *s;

	fputs(str, fl);
	fflush(fl);
	s = getText(fl2);
	if(s == NULL){
		return NULL;
	}

	return s;
}
