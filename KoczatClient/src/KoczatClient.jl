module KoczatClient

using Gtk, Sockets

struct Chat
	id:: UInt16
	isopen:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

struct User
	id:: UInt16
	isopen:: UInt8
	namelen:: UInt16
  name:: Ptr{UInt8}
end

struct Hello 
	op:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

struct HelloResp 
	op:: UInt8
	status:: UInt16
end

struct ForcedDisconnect 
	op:: UInt8
	reasonlen:: UInt16
	reason:: Ptr{UInt8}
end

struct GetUserList 
	op:: UInt8
end

struct GetUserListResp 
	op:: UInt8
	status:: UInt8
	userslen:: UInt16
	users:: Ptr{User}
end

struct GetChatList 
	op:: UInt8
end

struct GetChatListResp 
	op:: UInt8
	status:: UInt8
	chatslen:: UInt16
	chats:: Ptr{Chat}
end

struct CreateOpenChat 
	op:: UInt8
	namelen:: UInt16
	name:: Ptr{UInt8}
end

struct CreateOpenChatResp 
	op:: UInt8
	status:: UInt8
	id:: UInt16
end

struct CreatePasswordChat 
	op:: UInt8
	namelen:: UInt16
	passlen:: UInt16
	name_pass:: Ptr{UInt8}
end

struct CreatePasswordChatResp 
	op:: UInt8
	status:: UInt8
	id:: UInt16
end

struct JoinOpenChat 
	op:: UInt8
	id:: UInt16
end

struct JoinOpenChatResp 
	op:: UInt8
	status:: UInt8
end

struct JoinPasswordChat 
	op:: UInt8
	id:: UInt16
	passlen:: UInt16
	pass:: Ptr{UInt8}
end

struct JoinPasswordChatResp 
	op:: UInt8
	status:: UInt8
end

struct UserJoinedChat 
	op:: UInt8
	chatid:: UInt16
	userid:: UInt16
end

struct SendMessage 
	op:: UInt8
	chatid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

struct SendMessageResp 
	op:: UInt8
	status:: UInt8
end
    
struct ReceiveMessage 
	op:: UInt8
	chatid:: UInt16
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

struct SendDirect 
	op:: UInt8
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end

struct SendDirectResp 
	op:: UInt8
	status:: UInt8
end

struct ReceiveDirect 
	op:: UInt8
	userid:: UInt16
	msglen:: UInt16
	msg:: Ptr{UInt8}
end



const glade_xml = read(joinpath(@__DIR__, "../chatapp.glade"), String)

julia_main() = (main(); Cint(0))

function main(args = ARGS)
	global gtkbuilder = GtkBuilder(buffer = glade_xml)
	global window = gtkbuilder["application"]
	
	signal_connect(on_connect_button_clicked, gtkbuilder["connect_button"], :clicked)

  println("About to show window")
	showall(window)
end


asByteArray(x:: Integer) = reinterpret(UInt8, [x])
asByteArray(x:: String) = transcode(UInt8, x)

function on_connect_button_clicked(btn)
	println("[INFO] Connecting...")    
	ip = parse(IPAddr, get_gtk_property(gtkbuilder["ip_entry"], :text, String))
	port = parse(Int, get_gtk_property(gtkbuilder["port_entry"], :text, String))
	username = get_gtk_property(gtkbuilder["username_entry"], :text, String)
	
	println("[INFO] $ip:$port $username")
	len = asByteArray(hton(convert(UInt16, length(username))))
  req = vcat(b"\x01", len, asByteArray(username))
  println("[INFO] Request: $req")
	global conn = connect(ip, port)
	write(conn, req)
	bytes = readavailable(conn)
	
	println("Received: $bytes")
end

end # module
