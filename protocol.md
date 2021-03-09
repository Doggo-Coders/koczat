# Types
```
struct message {
	textsize: uint16
	authorsize: uint8
	text: char[textsize]
	author: char[authorsize]
}
```

```
enum hello_status {
	HELLO_OK = 1
	HELLO_NAME_TAKEN = 2
	HELLO_REJECTED = 3
}
```

```
enum send_message_status {
	SEND_MESSAGE_OK = 1
	SEND_MESSAGE_TOO_LONG = 2
	SEND_MESSAGE_REJECTED = 3
}
```

```
enum join_chat_status {
	JOIN_CHAT_OK = 1
	JOIN_CHAT_BAD_CHATID = 2
	JOIN_CHAT_REJECTED = 3
}
```

```
enum get_message_history_status {
	GET_MESSAGE_HISTORY_OK = 1
	GET_MESSAGE_HISTORY_BAD_CHATID = 2
	GET_MESSAGE_HISTORY_REJECTED = 3
}
```

# Client -> Server packets

## Hello packet
```
opcode: int8
namesize: uint8
name: char[namelen]
```
## SendMessage packet
```
opcode: int8
chatid: uint8
textsize: uint16
text: char[textsize]
```

## GetChats packet
```
opcode: int8
```

## JoinChat packet
```
opcode: int8
chatid: uint8
```

## GetMessageHistory packet
```
opcode: int8
chatid: uint8
```

## Disconnect packet
```
opcode: int8
```

# Server -> Client packets

## HelloReply packet
```
opcode: int8
status: enum hello_status (uint8)
```

## SendMessageReply packet
```
opcode: int8
status: enum send_message_status (uint8)
```

## JoinChatReply packet
```
opcode: int8
status: enum join_chat_status (uint8)
```

## GetMessageHistoryReply packet
```
opcode: int8
status: enum get_message_history_status (uint8)
messageslen: uint8
messages: struct message[messageslen]
```

## MessageSent packet
```
opcode: int8
chatid: uint8
message: struct message
```

## ForceDisconnect packet
```
opcode: int8
```





Client -> Server
```
Hello
GetChats
JoinChat
GetMessageHistory
SendMessage
```

Server -> Client
```
HelloReply
GetChatsReply
JoinChatReply
GetMessageHistoryReply
SendMessageReply
MessageReceived
ForceDisconnect
```
