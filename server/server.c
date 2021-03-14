#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef __unix__
#include <fcntl.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#else
#include <windows.h>
#endif

#include "../common.h"


#define MAX_CHATS 256
#define MAX_USERS 256
#define MAX_USER_NAME_LEN 63
#define MAX_REQ_SIZE 65536 // 64 KiB
#define SERVER_BACKLOG 1024 // idk
#define HELLO_PACKET_TIMEOUT_MS 250


struct chat {
	uint16_t namelen;
	char *name;
	uint16_t passlen; // 0 if no pass
	char *pass;       // NULL if no pass
	uint16_t users_count;
	uint8_t users[MAX_USERS/8];
};

struct user {
	int connfd;
	uint16_t namelen;
	char *name;
};


uint16_t g_chats_count, g_users_count;
uint8_t g_chats_ids[MAX_CHATS/8], g_users_ids[MAX_USERS/8];
struct chat g_chats[MAX_CHATS];
struct user g_users[MAX_USERS];


static inline int bitset_get(uint8_t *bitset, size_t ind);
static inline void bitset_set(uint8_t *bitset, size_t ind);
static void create_open_chat(uint16_t userid, const struct CreateOpenChat *req, struct CreateOpenChatResp *restrict resp);
static void create_password_chat(uint16_t userid, const struct CreatePasswordChat *req, struct CreatePasswordChatResp *restrict resp);
static void die(int exitcode, const char *fmt, ...);
static void join_open_chat(uint16_t userid, const struct JoinOpenChat *req, struct JoinOpenChatResp *restrict resp);
static void join_password_chat(uint16_t userid, const struct JoinPasswordChat *req, struct JoinPasswordChatResp *restrict resp);
static uint16_t gen_chatid(bool set);
static uint16_t gen_userid(bool set);
static int handle_disconnect(int connfd);
static int handle_new_connection(int connfd);
static int handle_packet(int connfd, void *buf, size_t reqsz);
static inline void log_info(const char *fmt, ...);
static void log_infov(const char *fmt, va_list v);
static inline void log_errno(int err);
static inline void log_error(const char *fmt, ...);
static void log_errorv(const char *fmt, va_list v);
static void main_loop();
static void receive_message(uint16_t userid, const struct SendMessage *req, struct ReceiveMessage *restrict resp);
static void send_message(uint16_t userid, const struct SendMessage *req, struct SendMessageResp *restrict resp);
static int send_packet(int connfd, void *data, size_t datasz);
static void sleep_millis(long millis);
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L
static inline char *strdup(const char *str);
static char *strndup(const char *str, size_t len);
#endif // _POSIX_C_SOURCE


int
bitset_get(uint8_t *bitset, size_t ind)
{
	return bitset[ind / 8] & (1 << ind % 8);
}

void
bitset_set(uint8_t *bitset, size_t ind)
{
	bitset[ind / 8] |= 1 << ind % 8;
}

void
create_open_chat(uint16_t userid, const struct CreateOpenChat *req, struct CreateOpenChatResp *restrict resp)
{
	uint16_t chatid;
	
	if ((chatid = gen_chatid(true)) == 0) {
		resp->status = STAT_CREATE_OPEN_CHAT_TOO_MANY;
		return;
	}
	
	g_chats[chatid-1].namelen = req->namelen;
	g_chats[chatid-1].name = strndup(req->name, req->namelen);
	g_chats[chatid-1].passlen = 0;
	g_chats[chatid-1].pass = NULL;
	g_chats[chatid-1].users_count = 0;
	memset(g_chats[chatid-1].users, 0, MAX_USERS/8);
	
	++g_chats_count;
	
	resp->status = STAT_OK;
	resp->id = chatid;
}

void
create_password_chat(uint16_t userid, const struct CreatePasswordChat *req, struct CreatePasswordChatResp *restrict resp)
{
	uint16_t chatid;
	
	if ((chatid = gen_chatid(true)) == 0) {
		resp->status = STAT_CREATE_PASSWORD_CHAT_TOO_MANY;
		return;
	}
	
	g_chats[chatid-1].namelen = req->namelen;
	g_chats[chatid-1].passlen = req->passlen;
	g_chats[chatid-1].name = strndup(req->name_pass, req->namelen);
	g_chats[chatid-1].pass = strndup(req->name_pass + req->namelen, req->passlen);
	g_chats[chatid-1].users_count = 0;
	memset(g_chats[chatid-1].users, 0, MAX_USERS/8);
	
	++g_chats_count;
	
	resp->status = STAT_OK;
	resp->id = chatid;
}

void
die(int exitcode, const char *fmt, ...)
{
	va_list v;
	va_start(v, fmt);
	printf("[FATAL]: ");
	vprintf(fmt, v);
	putchar('\n');
	va_end(v);
	exit(exitcode);
}

void
join_open_chat(uint16_t userid, const struct JoinOpenChat *req, struct JoinOpenChatResp *restrict resp)
{
	struct chat *chat;
	
	if (req->id > MAX_CHATS || !bitset_get(g_chats_ids, req->id)) {
		resp->status = STAT_JOIN_OPEN_CHAT_BAD_CHAT;
		return;
	}
	
	chat = g_chats + req->id - 1;
	
	if (bitset_get(chat->users, userid - 1)) {
		resp->status = STAT_JOIN_OPEN_CHAT_ALREADY_JOINED;
		return;
	}
	
	if (chat->pass) {
		resp->status = STAT_JOIN_OPEN_CHAT_NOT_OPEN;
		return;
	}
	
	++chat->users_count;
	bitset_set(chat->users, userid - 1);
	
	resp->status = STAT_OK;
}

void
join_password_chat(uint16_t userid, const struct JoinPasswordChat *req, struct JoinPasswordChatResp *restrict resp)
{
	struct chat *chat;
	
	if (req->id > MAX_CHATS || !bitset_get(g_chats_ids, req->id)) {
		resp->status = STAT_JOIN_PASSWORD_CHAT_BAD_CHAT;
		return;
	}
	
	chat = g_chats + req->id - 1;
	
	if (bitset_get(chat->users, userid - 1)) {
		resp->status = STAT_JOIN_PASSWORD_CHAT_ALREADY_JOINED;
		return;
	}
	
	if (!chat->pass) {
		resp->status = STAT_JOIN_PASSWORD_CHAT_NOT_PASSWORD;
		return;
	}
	
	if (
		chat->passlen != req->passlen ||
		strncmp(chat->pass, req->pass, req->passlen) != 0
	) {
		resp->status = STAT_JOIN_PASSWORD_CHAT_BAD_PASSWORD;
		return;
	}
	
	++chat->users_count;
	bitset_set(chat->users, userid - 1);
	
	resp->status = STAT_OK;
}

uint16_t
gen_chatid(bool set)
{
	for (int i = 0; i < MAX_CHATS/8; ++i) {
		for (int off = 7; off >= 0; --off) {
			if (!(g_chats_ids[i] & (1 << off))) {
				int ind = 8*i + off;
				if (set) {
					g_chats_ids[i] |= 1 << off;
				}
				return ind + 1;
			}
		}
	}
	return 0;
}

uint16_t
gen_userid(bool set)
{
	for (int i = 0; i < MAX_USERS/8; ++i) {
		for (int off = 7; off >= 0; --off) {
			if (!(g_users_ids[i] & (1 << off))) {
				int ind = 8*i + off;
				if (set) {
					g_users_ids[i] |= 1 << off;
				}
				return ind + 1;
			}
		}
	}
	return 0;
}

int
handle_disconnect(int connfd)
{
	
}

int
handle_new_connection(int connfd)
{
	int err;
	uint16_t userid;
	struct Hello *hello = malloc(sizeof(struct Hello) + MAX_USER_NAME_LEN + 1);
	struct HelloResp resp;
	resp.op = OP_HELLO_RESP;
	
	sleep_millis(HELLO_PACKET_TIMEOUT_MS);
	
	if (recv(connfd, &hello, sizeof(struct Hello) + MAX_USER_NAME_LEN, MSG_DONTWAIT) < 0) {
		err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK) {
			// Didn't receive Hello -> close conn
			log_info("New connection didn't send Hello. Closing connection.");
		} else if (err == ECONNREFUSED) {
			log_error("Connection refused when reading Hello. Closing connection.");
		}
		close(connfd);
		return 0;
	}
	
	if (g_users_count >= MAX_USERS) {
		resp.status = STAT_FU;
		send_packet(connfd, &resp, sizeof(struct HelloResp));
		close(connfd);
		return 0;
	}
	
	if (hello->op != OP_HELLO) {
		close(connfd);
		return 0;
	}
	
	hello->namelen = ntohs(hello->namelen);
	
	if (hello->namelen > MAX_USER_NAME_LEN) {
		resp.status = STAT_HELLO_NAME_TO_LONG;
		send_packet(connfd, &resp, sizeof(struct HelloResp));
		close(connfd);
		return 0;
	}
	
	// TODO: Validate names UTF-8/ASCII/non-NUL
	
	resp.status = STAT_OK;
	
	if (send_packet(connfd, &resp, sizeof(struct HelloResp)) < 0) {
		err = errno;
		switch (err) {
		case ECONNRESET:
			close(connfd);
			return 0;
		case ENOMEM:
			log_error("send() ran out of memory when sending HelloResp.");
			close(connfd);
			return 0;
		}
	}
	
	++g_users_count;
	userid = gen_userid(true);
	g_users[userid - 1] = (struct user) {
		.connfd = connfd,
		.namelen = hello->namelen,
		.name = strndup(hello->name, hello->namelen)
	};
	
	return 1;
}

int
handle_packet(int connfd, void *buf, size_t reqsz)
{
	uint16_t userid;
	int sendret, err;
	
	for (int i = 0; i < MAX_USERS; ++i) {
		if (bitset_get(g_users_ids, i)) {
			if (g_users[i].connfd == connfd) {
				userid = i + 1;
			}
		}
	}
	
	// TODO: cases for other opcodes
	// .op is always first
	switch (*(uint8_t *) buf) {
	case OP_CREATE_OPEN_CHAT: {
		struct CreateOpenChat *req = buf;
		struct CreateOpenChatResp resp;
		req->namelen = ntohs(req->namelen);
		if (reqsz < 1 + 2 + req->namelen) {
			goto invalid;
		}
		resp.op = OP_CREATE_OPEN_CHAT_RESP;
		create_open_chat(userid, buf, &resp);
		sendret = send_packet(connfd, &resp, sizeof(struct CreateOpenChatResp));
	} break;
	default:
		goto invalid;
	}
	
	if (sendret < 0) {
		err = errno;
		switch (err) {
		case ECONNRESET:
			// handle
			break;
		case ENOMEM:
			// handle
			break;
		}
	}
	
	goto skip;
invalid:
	// do sth if the request was invalid
skip:
	;
}

void
log_info(const char *fmt, ...)
{
	va_list v;
	va_start(v, fmt);
	log_infov(fmt, v);
	va_end(v);
}

void
log_infov(const char *fmt, va_list v)
{
	printf("[INFO ]: ");
	vprintf(fmt, v);
	putchar('\n');
}

void
log_errno(int err)
{
	log_error("errno %d: %s", err, strerror(err));
}

void
log_error(const char *fmt, ...)
{
	va_list v;
	va_start(v, fmt);
	log_errorv(fmt, v);
	va_end(v);
}

void log_errorv(const char *fmt, va_list v)
{
	printf("[ERROR]: ");
	vprintf(fmt, v);
	putchar('\n');
}

void
receive_message(uint16_t userid, const struct SendMessage *req, struct ReceiveMessage *restrict resp)
{
	// By now, we've already validated chatid in send_message()
	resp->userid = userid;
	resp->chatid = req->chatid;
	resp->msglen = req->msglen;
	memcpy(resp->msg, req->msg, req->msglen);
}

void
send_message(uint16_t userid, const struct SendMessage *req, struct SendMessageResp *restrict resp)
{
	struct chat *chat;
	
	if (req->chatid > MAX_CHATS || !bitset_get(g_chats_ids, req->chatid)) {
		resp->status = STAT_SEND_MESSAGE_BAD_CHAT;
		return;
	}
	
	chat = g_chats + req->chatid - 1;
	
	if (!bitset_get(chat->users, userid - 1)) {
		resp->status = STAT_SEND_MESSAGE_NOT_IN_CHAT;
		return;
	}
	
	resp->status = STAT_OK;
}

int
send_packet(int connfd, void *data, size_t datasz)
{
	return send(connfd, data, datasz, 0);
}

void
sleep_millis(long millis)
{
#ifdef __unix__
	struct timespec tm;
	tm.tv_sec = millis / 1000;
	tm.tv_nsec = millis * 1000000;
	while (nanosleep(&tm, &tm) < 0) {
		if (errno != EINTR) {
			break;
		}
	}
#else
	sleep(millis);
#endif
}

#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L

char *
strdup(const char *str)
{
	return strndup(str, strlen(str));
}

char *
strndup(const char *str, size_t len)
{
	char *s2 = malloc(len + 1);
	memcpy(s2, str, len);
	return s2;
}

#endif // _POSIX_C_SOURCE

void
main_loop(int port)
{
	uint8_t buf[MAX_REQ_SIZE];
	int err, ret;
	int serverfd, connfd;
	struct sockaddr_in sockaddr;
	fd_set readyfds, sockfds;
	
	memset(&sockaddr, 0, sizeof(struct sockaddr_in));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = INADDR_ANY;
	sockaddr.sin_port = htons(port);
	
	if ((serverfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		err = errno;
		switch (err) {
		case EACCES:
			die(-1, "No permission to open a TCP socket.");
		case EAFNOSUPPORT:
		case EINVAL:
			die(-1, "Your system doesn't support IPv4 family sockets.");
		case EMFILE:
			die(-1, "Process file descriptor limit reached.");
		case ENFILE:
			die(-1, "System file descriptor limit reached.");
		case ENOBUFS:
		case ENOMEM:
			die(137, "socket() ran out of memory.");
		case EPROTONOSUPPORT:
			die(-1, "Your system doesn't support TCP sockets.");
		}
	}
	
	if (bind(serverfd, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_in)) < 0) {
		err = errno;
		switch (err) {
		case EACCES:
			die(-1, "No permission to bind the TCP socket.");
		case EADDRINUSE:
			die(-1, "Socket address already in use.");
		}
	}
	
	if (listen(serverfd, SERVER_BACKLOG) < 0) {
		err = errno;
		switch (err) {
		case EADDRINUSE:
			die(-1, "Another socket is already listening on port %d.", port);
		}
	}
	
	FD_ZERO(&readyfds);
	FD_ZERO(&sockfds);
	
	FD_SET(serverfd, &sockfds);
	
	while (true) {
		readyfds = sockfds;
		
		if (select(FD_SETSIZE, &readyfds, NULL, NULL, NULL) < 0) {
			err = errno;
			if (err == ENOMEM) {
				die(137, "select() ran out of memory.");
			} else {
				log_errno(err);
			}
		}
		
		for (int i = 0; i < FD_SETSIZE; ++i) {
			if (FD_ISSET(i, &readyfds)) {
				if (i == serverfd) {
					// handle new conn
again:
					connfd = accept(serverfd, NULL, NULL);
					if (connfd < 0) {
						err = errno;
						if (err == EAGAIN || err == EWOULDBLOCK) {
							goto again;
						} else {
							log_errno(err);
						}
						continue;
					}
					if (handle_new_connection(connfd)) {
						FD_SET(connfd, &sockfds);
					} else {
						close(connfd);
					}
				} else {
					ret = recv(i, buf, MAX_REQ_SIZE, 0);
					if (ret < 0) {
						err = errno;
						log_errno(err);
					} else if (ret == 0) {
						// disconnect
						handle_disconnect(i);
						FD_CLR(i, &sockfds);
					} else {
						// handle packet
						handle_packet(i, buf, ret);
					}
				}
			}
		}
	}
}

int
main(int argc, char **argv)
{
	main_loop(8042);
	
	return 0;
}
