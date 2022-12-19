#ifndef SOCKETTOME_H_
#define SOCKETTOME_H_

int serve_socket(int port);
int accept_connection(int serverfd);
int request_connection(char *hn, int port);

#endif // SOCKETTOME_H_
