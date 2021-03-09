#ifndef _KOCZAT_SERVER_H_
#define _KOCZAT_SERVER_H_

#include <stddef.h>
#include <stdint.h>


#define MAX_MESSAGE_LEN 255
#define MAX_MESSAGES 256
#define MAX_TITLE_LEN 31
#define MAX_NAME_LEN 31
#define MAX_CHATS 16
#define SERVER_BACKLOG 100

#define OP_HELLO               1
#define OP_GET_CHATS           2
#define OP_JOIN_CHAT           3
#define OP_GET_MESSAGE_HISTORY 4
#define OP_SEND_MESSAGE        5

#define OP_HELLO_REPLY               (-1)
#define OP_GET_CHATS_REPLY           (-2)
#define OP_JOIN_CHAT_REPLY           (-3)
#define OP_GET_MESSAGE_HISTORY_REPLY (-4)
#define OP_SEND_MESSAGE_REPLY        (-5)
#define OP_MESSAGE_RECEIVED          (-6)
#define OP_FORCE_DISCONNECT          (-7)


struct message {
	char text[MAX_MESSAGE_LEN + 1];
	char author[MAX_NAME_LEN + 1];
};

struct chat {
	char title[MAX_TITLE_LEN + 1];
	struct message messages[MAX_MESSAGES];
	int next_msg_ind;
};

enum hello_status {
	HELLO_OK = 1,
	HELLO_NAME_TAKEN = 2,
	HELLO_REJECTED = 3
};

enum get_chats_status {
	GET_CHATS_OK = 1,
	GET_CHATS_REJECTED = 3
};

enum join_chat_status {
	JOIN_CHAT_OK = 1,
	JOIN_CHAT_BAD_CHATID = 2,
	JOIN_CHAT_REJECTED = 3
};

enum get_message_history_status {
	GET_MESSAGE_HISTORY_OK = 1,
	GET_MESSAGE_HISTORY_BAD_CHATID = 2,
	GET_MESSAGE_HISTORY_REJECTED = 3
};

enum send_message_status {
	SEND_MESSAGE_OK = 1,
	SEND_MESSAGE_TOO_LONG = 2,
	SEND_MESSAGE_REJECTED = 3
};


extern struct chat *g_chats[MAX_CHATS];


void change_title(struct chat *chat, const char *title);
void send_message(struct chat *chat, const struct message *msg);

void server(int port);


#endif
