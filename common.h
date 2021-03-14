#include <stdint.h>

#define STAT_OK 0
#define STAT_FU 1

struct Chat {
	uint16_t id;
	uint8_t isopen;
	uint16_t namelen;
	char name[];
};

struct User {
	uint16_t id;
	uint16_t namelen;
	char name[];
};


#define OP_HELLO 1

struct Hello {
	uint8_t op;
	uint16_t namelen;
	char name[];
};

#define OP_HELLO_RESP 2

#define STAT_HELLO_NAME_TO_LONG 2
#define STAT_HELLO_BAD_NAME 3

struct HelloResp {
	uint8_t op;
	uint8_t status;
};

#define OP_FORCED_DISCONNECT 3

struct ForcedDisconnect {
	uint8_t op;
	uint16_t reasonlen;
	char reason[];
};

#define OP_GET_USER_LIST 4

struct GetUserList {
	uint8_t op;
};

#define OP_GET_USER_LIST_RESP 5

struct GetUserListResp {
	uint8_t op;
	uint8_t status;
	uint16_t userslen;
	struct User users[];
};

#define OP_GET_CHAT_LIST 6

struct GetChatList {
	uint8_t op;
};

#define OP_GET_CHAT_LIST_RESP 7

struct GetChatListResp {
	uint8_t op;
	uint8_t status;
	uint16_t chatslen;
	struct Chat chats[];
};

#define OP_CREATE_OPEN_CHAT 8

struct CreateOpenChat {
	uint8_t op;
	uint16_t namelen;
	char name[];
};

#define OP_CREATE_OPEN_CHAT_RESP 9

#define STAT_CREATE_OPEN_CHAT_TOO_MANY 2
#define STAT_CREATE_OPEN_CHAT_BAD_NAME 3

struct CreateOpenChatResp {
	uint8_t op;
	uint8_t status;
	uint16_t id;
};

#define OP_CREATE_PASSWORD_CHAT 10

struct CreatePasswordChat {
	uint8_t op;
	uint16_t namelen;
	uint16_t passlen;
	char name_pass[];
};

#define OP_CREATE_PASSWORD_CHAT_RESP 11

#define STAT_CREATE_PASSWORD_CHAT_TOO_MANY 2
#define STAT_CREATE_PASSWORD_CHAT_BAD_NAME 3

struct CreatePasswordChatResp {
	uint8_t op;
	uint8_t status;
	uint16_t id;
};

#define OP_JOIN_OPEN_CHAT 12

struct JoinOpenChat {
	uint8_t op;
	uint16_t id;
};

#define OP_JOIN_OPEN_CHAT_RESP 13

#define STAT_JOIN_OPEN_CHAT_BAD_CHAT       2
#define STAT_JOIN_OPEN_CHAT_ALREADY_JOINED 3
#define STAT_JOIN_OPEN_CHAT_NOT_OPEN       4

struct JoinOpenChatResp {
	uint8_t op;
	uint8_t status;
};

#define OP_JOIN_PASSWORD_CHAT 14

struct JoinPasswordChat {
	uint8_t op;
	uint16_t id;
	uint16_t passlen;
	char pass[];
};

#define OP_JOIN_PASSWORD_CHAT_RESP 15

#define STAT_JOIN_PASSWORD_CHAT_BAD_CHAT       2
#define STAT_JOIN_PASSWORD_CHAT_ALREADY_JOINED 3
#define STAT_JOIN_PASSWORD_CHAT_NOT_PASSWORD   4
#define STAT_JOIN_PASSWORD_CHAT_BAD_PASSWORD   5

struct JoinPasswordChatResp {
	uint8_t op;
	uint8_t status;
};

#define OP_USER_JOINED_CHAT 16

struct UserJoinedChat {
	uint8_t op;
	uint16_t chatid;
	uint16_t userid;
};

#define OP_SEND_MESSAGE 17

struct SendMessage {
	uint8_t op;
	uint16_t chatid;
	uint16_t msglen;
	char msg[];
};

#define OP_SEND_MESSAGE_RESP 18

#define STAT_SEND_MESSAGE_BAD_CHAT    2
#define STAT_SEND_MESSAGE_NOT_IN_CHAT 3

struct SendMessageResp {
	uint8_t op;
	uint8_t status;
};

#define OP_RECEIVE_MESSAGE 19

struct ReceiveMessage {
	uint8_t op;
	uint16_t chatid;
	uint16_t userid;
	uint16_t msglen;
	char msg[];
};

#define OP_SEND_DIRECT 20

struct SendDirect {
	uint8_t op;
	uint16_t userid;
	uint16_t msglen;
	char msg[];
};

#define OP_SEND_DIRECT_RESP 21

struct SendDirectResp {
	uint8_t op;
	uint8_t status;
};

#define OP_RECEIVE_DIRECT 22

struct ReceiveDirect {
	uint8_t op;
	uint16_t userid;
	uint16_t msglen;
	char msg[];
};
