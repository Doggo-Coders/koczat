#include <string.h>

#include "server.h"


void change_title(struct chat *chat, const char *title);
void send_message(struct chat *chat, const struct message *msg);


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


int
main(int argc, char **argv)
{
	server(8042);
}
