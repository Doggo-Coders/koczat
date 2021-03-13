# Primitive types
type ChatId = uint16
type str = (uint32, char[])
type bool = uint8
type UserId = uint16

# Basics

Alice -> Hello name:str -> Server -> HelloResp id:UserId -> Alice
Server -> ForcedDisconnect -> Alice
# Users

Alice -> GetUserList -> Server -> GetUserListResp users:User[] -> Alice

```
struct User {
	id: UserId
	name: str
}
```

# Chats

Alice -> GetChatList -> Server -> GetChatListResp chats:Chat[] -> Alice

```
struct Chat {
	isopen: bool
	name: str
	id: ChatId
}
```

## Type of chats:

### Open Chats
Alice -> CreateOpenChat name:str -> Server -> CreateOpenChatResp id:ChatId -> Alice
Bob -> JoinOpenChat id:ChatId -> (Server (-> JoinOpenChatResp -> Bob) AND (-> UserJoinedChat user:UserId Chat:ChatId) => Group)

### Password protected chats
Alice -> CreatePasswordChat name:str pass:str -> Server -> CreatePasswordChatResp id:ChatId -> Alice
Bob -> JoinPasswordChat id:ChatId pass:str -> (Server (-> JoinPasswordChatResp -> Bob) AND (-> UserJoinedChat user:UserId chat:ChatId) => Group)

### Direct Chats
Alice -> SendDirect to:UserId msg:str -> (Server (-> SendDirectResp -> Alice) AND (-> ReceiveDirect from:UserId msg:str -> Bob))

# Messages
Alice -> SendMessage id:ChatId msg:str -> (Server (-> SendMessageResp -> Alice) AND (-> ReceiveMessage id:ChatId msg:str => Group))


# Opcodes

| Value | Name |
|-------|------|
|  1   | Hello |
|  2   | HelloResp |
|  3   | ForcedDisconnect |
|  4   | GetUserList |
|  5   | GetUserListResp |
|  6   | GetChatList |
|  7   | GetChatListResp |
|  8   | CreateOpenChat |
|  9   | CreateOpenChatResp |
|  10  | CreatePasswordChat |
|  11  | CreatePasswordChatResp |
|  12  | JoinOpenChat |
|  13  | JoinOpenChatResp |
|  14  | JoinPasswordChat |
|  15  | JoinPasswordChatResp |
|  16  | UserJoinedChat |
|  17  | SendMessage |
|  18  | SendMessageResp |
|  19  | ReceiveMessage |
|  20  | SendDirect |
|  21  | SendDirectResp |
|  22  | ReceiveDirect |
