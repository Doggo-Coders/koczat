#include <string.h>

#ifdef __unix__
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#endif

/** Macros **/

#define MAX_MESSAGE_LEN 255
#define MAX_MESSAGES 256
#define MAX_TITLE_LEN 31
#define MAX_NAME_LEN 31
#define MAX_CHATS 16
#define SERVER_BACKLOG 100

/** Type definitions **/

struct message {
	char text[MAX_MESSAGE_LEN + 1];
	char author[MAX_NAME_LEN + 1];
};

struct chat {
	char title[MAX_TITLE_LEN + 1];
	struct message messages[MAX_MESSAGES];
	int next_msg_ind;
};

#ifdef __unix__
struct connection {
	int fd;
	char username[MAX_NAME_LEN + 1];
	struct connection *_prev, *_next;
};
#endif

/** Global variables **/

struct chat *g_chats[MAX_CHATS];

#ifdef __unix__
int g_server_fd;
struct connection *g_connections;
#endif

/** Function declarations **/

static void change_title(struct chat *chat, const char *title);
static void send_message(struct chat *chat, const struct message *msg);
static void server(int port);

#ifdef __unix__
static inline void server_loop();
static void accept_new_connection(int connfd);
static int read_from_connection(struct connection *conn);
static void close_connection(struct connection *conn);
static struct connection *get_connection_by_fd(int connfd);
#endif

/** Function definitions **/

void
change_title(struct chat *chat, const char *title)
{
	strncpy(chat->title, title, MAX_TITLE_LEN);
}

void
send_message(struct chat *chat, const struct message *msg)
{
	struct message *message = &chat->messages[chat->next_msg_ind];
	
	strncpy(message->text, msg->text, MAX_MESSAGE_LEN);
	strncpy(message->author, msg->author, MAX_NAME_LEN);
	
	++chat->next_msg_ind;
	chat->next_msg_ind %= MAX_MESSAGES;
}

#ifdef __unix__

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

#endif // __unix__

int
main(int argc, char **argv)
{
	server(8042);
}
