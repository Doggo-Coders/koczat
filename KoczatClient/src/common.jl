#=
Koczat - Server - TCP Chat
Copyright (C) 2021 Tomasz "TFKls" Kulis and Łukasz "Masflam" Drukała

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
=#

const STAT_OK = UInt8(0)
const STAT_FU = UInt8(1)

struct Chat
	id:: UInt16
	isopen:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

struct User
	id:: UInt16
	namelen:: UInt16
	name:: Ptr{UInt8}
end

const OP_HELLO = UInt8(1)

struct Hello
	op:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

const OP_HELLO_RESP = UInt8(2)

const STAT_HELLO_NAME_TO_LONG = UInt8(2)
const STAT_HELLO_BAD_NAME =     UInt8(3)

struct HelloResp
	op:: UInt8
	status:: UInt16
end

const OP_FORCED_DISCONNECT = UInt8(3)

struct ForcedDisconnect
	op:: UInt8
	reasonlen:: UInt16
	reason:: Ptr{UInt8}
end

const OP_GET_USER_LIST = UInt8(4)

struct GetUserList
	op:: UInt8
end

const OP_GET_USER_LIST_RESP = UInt8(5)

struct GetUserListResp
	op:: UInt8
	status:: UInt8
	userslen:: UInt16
	users:: Ptr{User}
end

const OP_GET_CHAT_LIST = UInt8(6)

struct GetChatList
	op:: UInt8
end

const OP_GET_CHAT_LIST_RESP = UInt8(7)

struct GetChatListResp
	op:: UInt8
	status:: UInt8
	chatslen:: UInt16
	chats:: Ptr{Chat}
end

const OP_CREATE_OPEN_CHAT = UInt8(8)

struct CreateOpenChat
	op:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

const OP_CREATE_OPEN_CHAT_RESP = UInt8(9)

const STAT_CREATE_OPEN_CHAT_TOO_MANY = UInt8(2)
const STAT_CREATE_OPEN_CHAT_BAD_NAME = UInt8(3)

struct CreateOpenChatResp
	op:: UInt8
	status:: UInt8
	id:: UInt16
end

const OP_CREATE_PASSWORD_CHAT = UInt8(10)

struct CreatePasswordChat
	op:: UInt8
	namelen:: UInt16
	passlen:: UInt16
	name_pass:: Ptr{UInt8}
end

const OP_CREATE_PASSWORD_CHAT_RESP = UInt8(11)

const STAT_CREATE_PASSWORD_CHAT_TOO_MANY = UInt8(2)
const STAT_CREATE_PASSWORD_CHAT_BAD_NAME = UInt8(3)

struct CreatePasswordChatResp
	op:: UInt8
	status:: UInt8
	id:: UInt16
end

const OP_JOIN_OPEN_CHAT = UInt8(12)

struct JoinOpenChat
	op:: UInt8
	id:: UInt16
end

const OP_JOIN_OPEN_CHAT_RESP = UInt8(13)

const STAT_JOIN_OPEN_CHAT_BAD_CHAT =       UInt8(2)
const STAT_JOIN_OPEN_CHAT_ALREADY_JOINED = UInt8(3)
const STAT_JOIN_OPEN_CHAT_NOT_OPEN =       UInt8(4)

struct JoinOpenChatResp
	op:: UInt8
	status:: UInt8
end

const OP_JOIN_PASSWORD_CHAT = UInt8(14)

struct JoinPasswordChat
	op:: UInt8
	id:: UInt16
	passlen:: UInt16
	pass:: Ptr{UInt8}
end

const OP_JOIN_PASSWORD_CHAT_RESP = UInt8(15)

const STAT_JOIN_PASSWORD_CHAT_BAD_CHAT =       UInt8(2)
const STAT_JOIN_PASSWORD_CHAT_ALREADY_JOINED = UInt8(3)
const STAT_JOIN_PASSWORD_CHAT_NOT_PASSWORD =   UInt8(4)
const STAT_JOIN_PASSWORD_CHAT_BAD_PASSWORD =   UInt8(5)

struct JoinPasswordChatResp
	op:: UInt8
	status:: UInt8
end

const OP_USER_JOINED_CHAT = UInt8(16)

struct UserJoinedChat
	op:: UInt8
	chatid:: UInt16
	userid:: UInt16
end

const OP_SEND_MESSAGE = UInt8(17)

struct SendMessage
	op:: UInt8
	chatid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

const OP_SEND_MESSAGE_RESP = UInt8(18)

const STAT_SEND_MESSAGE_BAD_CHAT =    UInt8(2)
const STAT_SEND_MESSAGE_NOT_IN_CHAT = UInt8(3)

struct SendMessageResp
	op:: UInt8
	status:: UInt8
end

const OP_RECEIVE_MESSAGE = UInt8(19)

struct ReceiveMessage
	op:: UInt8
	chatid:: UInt16
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

const OP_SEND_DIRECT = UInt8(20)

struct SendDirect
	op:: UInt8
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

const OP_SEND_DIRECT_RESP = UInt8(21)

const STAT_SEND_DIRECT_BAD_USER = UInt8(2)

struct SendDirectResp
	op:: UInt8
	status:: UInt8
end

const OP_RECEIVE_DIRECT = UInt8(22)

struct ReceiveDirect
	op:: UInt8
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end
