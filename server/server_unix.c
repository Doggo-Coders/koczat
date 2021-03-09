#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "server.h"


struct connection {
	int fd;
	char username[MAX_NAME_LEN + 1];
	uint16_t chatid_mask;
	struct connection *_prev, *_next;
};


int g_server_fd;
struct connection *g_connections;


static inline void server_loop();
static void accept_new_connection(int connfd);
static int read_from_connection(struct connection *conn);
static void close_connection(struct connection *conn);
static struct connection *get_connection_by_fd(int connfd);


void
server(int port)
{
	struct sockaddr_in servaddr;
	
	g_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	
	memset(&servaddr, 0, sizeof(struct sockaddr_in));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);
	servaddr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(g_server_fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0) {
		// error
	}
	
	if (listen(g_server_fd, SERVER_BACKLOG) < 0) {
		// error
	}
	
	server_loop();
}

void
server_loop()
{
	fd_set readyfds, sockfds;
	
	FD_ZERO(&readyfds);
	FD_ZERO(&sockfds);
	
	FD_SET(g_server_fd, &sockfds);
	
	while (1) {
		readyfds = sockfds;
		
		if (select(FD_SETSIZE, &readyfds, NULL, NULL, NULL) < 0) {
			// error
		}
		
		for (int i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET(i, &readyfds)) {
				if (i == g_server_fd) {
					int connfd = accept(g_server_fd, NULL, NULL);
					if (connfd < 0) {
						// error
					}
					accept_new_connection(connfd);
					FD_SET(connfd, &sockfds);
				} else {
					struct connection *conn = get_connection_by_fd(i); // never gonna be NULL
					int closed = read_from_connection(conn);
					if (closed) {
						close_connection(conn);
						FD_CLR(i, &sockfds);
					}
				}
			}
		}
	}
}

void
accept_new_connection(int connfd)
{
	int o;
	uint8_t opcode, namesize;
	char name[MAX_NAME_LEN];
	
	o = read(connfd, &opcode, 1);
	if (o < 0) {
		// read error
	} else if (o < 1) {
		// there was no byte
	}
	
	if (opcode != OP_HELLO) {
		// close down the conn
	}
	
	o = read(connfd, &namesize, 1);
	if (o < 0) {
		// read error
	} else if (o < 1) {
		// there was no byte
	}
	
	read(connfd, name, namesize);
}

int
read_from_connection(struct connection *conn)
{
	
}

struct connection *
get_connection_by_fd(int fd)
{
	struct connection *conn = g_connections, *prev = NULL;
	while (conn != NULL) {
		if (conn->fd == fd) {
			return conn;
		} else {
			prev = conn;
			conn = conn->_next;
		}
	}
	return prev;
}

void
close_connection(struct connection *conn)
{
	
}
