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
#include <netinet/in.h>
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
static struct GetUserListResp *get_user_list(uint16_t userid, const struct GetUserList *req, struct GetUserListResp *resp, size_t *restrict respsz);
static struct GetChatListResp *get_chat_list(uint16_t userid, const struct GetChatList *req, struct GetChatListResp *resp, size_t *restrict respsz);
static int handle_disconnect(int connfd);
static int handle_new_connection(int connfd);
static int handle_packet(int connfd, void *buf, size_t reqsz);
static inline void log_info(const char *fmt, ...);
static void log_infov(const char *fmt, va_list v);
static inline void log_errno(int err);
static inline void log_error(const char *fmt, ...);
static void log_errorv(const char *fmt, va_list v);
static void main_loop();
static void receive_direct(uint16_t userid, const struct SendDirect *req, struct ReceiveDirect *restrict resp);
static void receive_message(uint16_t userid, const struct SendMessage *req, struct ReceiveMessage *restrict resp);
static void send_direct(uint16_t userid, const struct SendDirect *req, struct SendDirectResp *restrict resp);
static void send_message(uint16_t userid, const struct SendMessage *req, struct SendMessageResp *restrict resp);
static int send_packet(int connfd, void *data, size_t datasz);
static void send_receive_direct(uint16_t userid, const struct SendDirect *req);
static void send_receive_message(uint16_t userid, const struct SendMessage *req);
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

struct GetChatListResp *
get_chat_list(uint16_t userid, const struct GetChatList *req, struct GetChatListResp *resp, size_t *restrict respsz)
{
	int p = 0;
	size_t sz = sizeof(struct GetChatListResp);
	
	resp->status = STAT_OK;
	resp->chatslen = htons(g_chats_count);
	
	for (int i = 0; i < g_chats_count; ++i) {
		if (bitset_get(g_chats_ids, i)) {
			resp = realloc(resp, sz+= sizeof(struct Chat) + g_chats[i].namelen);
			resp->chats[i].id = htons(i + 1);
			resp->chats[i].isopen = g_chats[i].pass == NULL;
			resp->chats[i].namelen = htons(g_chats[i].namelen);
			memcpy(resp->chats[p].name, g_chats[i].name, g_chats[i].namelen);
			++p;
		}
	}
	
	*respsz = sz;
	return resp;
}

struct GetUserListResp *
get_user_list(uint16_t userid, const struct GetUserList *req, struct GetUserListResp *resp, size_t *restrict respsz)
{
	int p = 0;
	size_t sz = sizeof(struct GetUserListResp);
	
	resp->status = STAT_OK;
	resp->userslen = htons(g_users_count);
	
	for (int i = 0; i < MAX_USERS; ++i) {
		if (bitset_get(g_users_ids, i)) {
			resp = realloc(resp, sz += sizeof(struct User) + g_users[i].namelen);
			resp->users[p].id = htons(i + 1);
			resp->users[p].namelen = htons(g_users[i].namelen);
			memcpy(resp->users[p].name, g_users[i].name, g_users[i].namelen);
			++p;
		}
	}
	
	*respsz = sz;
	return resp;
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
	case OP_GET_USER_LIST: {
		struct GetUserList *req = buf; // already >= minimum size of 1 byte
		struct GetUserListResp *resp = malloc(sizeof(struct GetUserListResp));
		size_t respsz;
		resp->op = OP_GET_USER_LIST_RESP;
		resp = get_user_list(userid, req, resp, &respsz);
		sendret = send_packet(connfd, resp, respsz);
		free(resp);
	} break;
	case OP_GET_CHAT_LIST: {
		struct GetChatList *req = buf; // already >= minimum size of 1 byte
		struct GetChatListResp *resp = malloc(sizeof(struct GetChatListResp));
		size_t respsz;
		resp->op = OP_GET_CHAT_LIST_RESP;
		resp = get_chat_list(userid, req, resp, &respsz);
		sendret = send_packet(connfd, resp, respsz);
		free(resp);
	} break;
	case OP_CREATE_OPEN_CHAT: {
		struct CreateOpenChat *req = buf;
		struct CreateOpenChatResp resp;
		if (reqsz < 1 + 2 || reqsz < 1 + 2 + (req->namelen = ntohs(req->namelen))) {
			goto invalid;
		}
		resp.op = OP_CREATE_OPEN_CHAT_RESP;
		create_open_chat(userid, req, &resp);
		resp.id = htons(resp.id);
		sendret = send_packet(connfd, &resp, sizeof(struct CreateOpenChatResp));
	} break;
	case OP_CREATE_PASSWORD_CHAT: {
		struct CreatePasswordChat *req = buf;
		struct CreatePasswordChatResp resp;
		if (
			reqsz < 1 + 2 + 2 ||
			reqsz < 1 + 2 + 2 +
				(req->namelen = ntohs(req->namelen)) +
				(req->passlen = ntohs(req->passlen))
		) {
			goto invalid;
		}
		resp.op = OP_CREATE_PASSWORD_CHAT_RESP;
		create_password_chat(userid, req, &resp);
		resp.id = htons(resp.id);
		sendret = send_packet(connfd, &resp, sizeof(struct CreatePasswordChatResp));
	} break;
	case OP_JOIN_OPEN_CHAT: {
		struct JoinOpenChat *req = buf;
		struct JoinOpenChatResp resp;
		if (reqsz < 1 + 2) {
			goto invalid;
		}
		req->id = ntohs(req->id);
		resp.op = OP_JOIN_OPEN_CHAT_RESP;
		join_open_chat(userid, req, &resp);
		sendret = send_packet(connfd, &resp, sizeof(struct JoinOpenChatResp));
		// TODO: UserJoinedChat
	} break;
	case OP_JOIN_PASSWORD_CHAT: {
		struct JoinPasswordChat *req = buf;
		struct JoinPasswordChatResp resp;
		if (reqsz < 1 + 2 + 2 || reqsz < 1 + 2 + 2 + (req->passlen = ntohs(req->passlen))) {
			goto invalid;
		}
		req->id = ntohs(req->id);
		resp.op = OP_JOIN_PASSWORD_CHAT_RESP;
		join_password_chat(userid, req, &resp);
		sendret = send_packet(connfd, &resp, sizeof(struct JoinPasswordChatResp));
		// TODO: UserJoinedChat
	} break;
	case OP_SEND_MESSAGE: {
		struct SendMessage *req = buf;
		struct SendMessageResp resp;
		if (reqsz < 1 + 2 + 2 || reqsz < 1 + 2 + 2 + (req->msglen = ntohs(req->msglen))) {
			goto invalid;
		}
		req->chatid = ntohs(req->chatid);
		resp.op = OP_SEND_MESSAGE_RESP;
		send_message(userid, req, &resp);
		sendret = send_packet(connfd, &resp, sizeof(struct SendMessageResp));
		send_receive_message(userid, req);
	} break;
	case OP_SEND_DIRECT: {
		struct SendDirect *req = buf;
		struct SendDirectResp resp;
		if (reqsz < 1 + 2 + 2 || reqsz < 1 + 2 + 2 + (req->msglen = ntohs(req->msglen))) {
			goto invalid;
		}
		req->userid = ntohs(req->userid);
		resp.op = OP_SEND_DIRECT_RESP;
		send_direct(userid, req, &resp);
		sendret = send_packet(connfd, &resp, sizeof(struct SendDirectResp));
		send_receive_direct(userid, req);
	} break;
	default:
		goto invalid;
	}
	
	if (sendret < 0) {
		err = errno;
		switch (err) {
		case ECONNRESET:
			log_error("Client connection reset when responding/notifying.");
			return 0;
		case ENOMEM:
			log_error("send() ran out of memory when responding/notifying.");
			return 0;
		}
	}
	
	goto skip;
invalid:
	log_info("Client sent invalid request.");
	return 0;
skip:
	return 1;
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
	memcpy(resp->msg, req->msg, req->msglen);
	resp->userid = htons(userid);
	resp->chatid = htons(req->chatid);
	resp->msglen = htons(req->msglen);
}

void
receive_direct(uint16_t userid, const struct SendDirect *req, struct ReceiveDirect *restrict resp)
{
	memcpy(resp->msg, req->msg, req->msglen);
	resp->userid = htons(userid);
	resp->msglen = htons(req->msglen);
}

void
send_direct(uint16_t userid, const struct SendDirect *req, struct SendDirectResp *restrict resp)
{
	if (req->userid > MAX_USERS || !bitset_get(g_users_ids, req->userid)) {
		resp->status = STAT_SEND_DIRECT_BAD_USER;
		return;
	}
	
	resp->status = STAT_OK;
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
send_receive_direct(uint16_t userid, const struct SendDirect *req)
{
	// req is all validated and byteswapped here already
	int err;
	struct ReceiveDirect *notif = malloc(sizeof(struct ReceiveDirect) + req->msglen);
	notif->op = OP_RECEIVE_DIRECT;
	receive_direct(userid, req, notif);
	
	if (send_packet(g_users[req->userid].connfd, notif, sizeof(struct ReceiveDirect) + req->msglen) < 0) {
		err = errno;
		switch (err) {
		case ECONNRESET:
			log_error("Client connection reset when sending ReceiveDirect.");
			break;
		case ENOMEM:
			log_error("send() ran out of memory when sending ReceiveDirect.");
			break;
		}
	}
	
	free(notif);
}

void
send_receive_message(uint16_t userid, const struct SendMessage *req)
{
	// req is all validated and byteswapped here already
	int err;
	struct ReceiveMessage *notif = malloc(sizeof(struct ReceiveMessage) + req->msglen);
	notif->op = OP_RECEIVE_MESSAGE;
	receive_message(userid, req, notif);
	
	for (int i = 0; i < MAX_USERS; ++i) {
		if (bitset_get(g_chats[req->chatid].users, i)) {
			// don't send to sender, they've already gotten the SendMessageResp
			if (i + 1 == userid) {
				continue;
			}
			
			if (send_packet(g_users[i].connfd, notif, sizeof(struct ReceiveMessage) + req->msglen) < 0) {
				err = errno;
				switch (err) {
				case ECONNRESET:
					log_error("Client connection reset when sending ReceiveMessage.");
					break;
				case ENOMEM:
					log_error("send() ran out of memory when sending ReceiveMessage.");
					break;
				}
			}
		}
	}
	
	free(notif);
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
usagedie()
{
	printf("[FATAL]: Usage: koczat-server [--port|-p <port=8042>]");
	die(2, "");
}

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
			die(77, "No permission to open a TCP socket.");
		case EAFNOSUPPORT:
		case EINVAL:
			die(69, "Your system doesn't support IPv4 family sockets.");
		case EMFILE:
			die(71, "Process file descriptor limit reached.");
		case ENFILE:
			die(71, "System file descriptor limit reached.");
		case ENOBUFS:
		case ENOMEM:
			die(137, "socket() ran out of memory.");
		case EPROTONOSUPPORT:
			die(69, "Your system doesn't support TCP sockets.");
		}
	}
	
	if (bind(serverfd, (struct sockaddr *) &sockaddr, sizeof(struct sockaddr_in)) < 0) {
		err = errno;
		switch (err) {
		case EACCES:
			die(77, "No permission to bind the TCP socket.");
		case EADDRINUSE:
			die(74, "Socket address already in use.");
		}
	}
	
	if (listen(serverfd, SERVER_BACKLOG) < 0) {
		err = errno;
		switch (err) {
		case EADDRINUSE:
			die(74, "Another socket is already listening on port %d.", port);
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
						if (!handle_packet(i, buf, ret)) {
							close(i);
							FD_CLR(i, &sockfds);
						}
					}
				}
			}
		}
	}
}

int
main(int argc, char **argv)
{
	int port = 8042;
	
	for (int i = 1; i < argc; ++i) {
		char *arg = argv[i];
		
		if (strcmp(arg, "--port") == 0 || strcmp(arg, "-p") == 0) {
			if (i == argc-1) usagedie();
			arg = argv[++i];
			sscanf(arg, "%d", &port);
		}
	}
	
	main_loop(port);
	
	return 0;
}
