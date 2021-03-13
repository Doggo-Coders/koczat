#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common.h"


#define MAX_CHATS 256
#define MAX_USERS 256


struct chat {
	uint16_t namelen;
	char *name;
	uint16_t passlen; // 0 if no pass
	char *pass;       // NULL if no pass
	uint16_t users_count;
	uint8_t users[MAX_USERS/8];
};

struct user {
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
#if !defined(_POSIX_C_SOURCE) || _POSIX_C_SOURCE < 200809L
static inline char *strdup(const char *str);
static char *strndup(const char *str, size_t len);
#endif // _POSIX_C_SOURCE
static void join_open_chat(uint16_t userid, const struct JoinOpenChat *req, struct JoinOpenChatResp *restrict resp);
static void join_password_chat(uint16_t userid, const struct JoinPasswordChat *req, struct JoinPasswordChatResp *restrict resp);
static uint16_t gen_chatid(bool set);
static void receive_message(uint16_t userid, const struct SendMessage *req, struct ReceiveMessage *restrict resp);
static void send_message(uint16_t userid, const struct SendMessage *req, struct SendMessageResp *restrict resp);


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
main(int argc, char **argv)
{
	
	
	return 0;
}
