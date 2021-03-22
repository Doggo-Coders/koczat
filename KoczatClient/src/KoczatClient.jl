module KoczatClient

using Gtk, Sockets, Logging

include("common.jl")

struct Message
	userid:: UInt16
	username:: String
	msg:: String
end

const glade_xml = read(joinpath(@__DIR__, "../chatapp.glade"), String)

gtkbuilder = nothing
window = nothing
conn = nothing
ouruserid = nothing
ourusername = nothing
user_list_store = nothing
chat_list_store = nothing
message_list_store = nothing
chat_messages = nothing
current_chat = nothing
direct_list_store = nothing

julia_main() = (main(); Cint(0))
function main(args = ARGS)
	global gtkbuilder = GtkBuilder(buffer = glade_xml)
	global window = gtkbuilder["application"]
	global user_list_store = GtkListStore(UInt16, String)
	global chat_list_store = GtkListStore(UInt16, Bool, String, Bool)
	global message_list_store = GtkListStore(UInt16, String, String)
	global chat_messages = Dict{UInt16, Vector{Message}}()
	global direct_list_store = GtkListStore(UInt16, String, UInt16, String, String)
	
	GAccessor.model(gtkbuilder["user_list"], GtkTreeModel(user_list_store))
	push!(gtkbuilder["user_list"], GtkTreeViewColumn("ID", GtkCellRendererText(), Dict([("text", 0)])))
	push!(gtkbuilder["user_list"], GtkTreeViewColumn("Name", GtkCellRendererText(), Dict([("text", 1)])))
	
	GAccessor.model(gtkbuilder["chat_list"], GtkTreeModel(chat_list_store))
	push!(gtkbuilder["chat_list"], GtkTreeViewColumn("ID", GtkCellRendererText(), Dict([("text", 0)])))
	push!(gtkbuilder["chat_list"], GtkTreeViewColumn("Open", GtkCellRendererText(), Dict([("text", 1)])))
	push!(gtkbuilder["chat_list"], GtkTreeViewColumn("Name", GtkCellRendererText(), Dict([("text", 2)])))
	push!(gtkbuilder["chat_list"], GtkTreeViewColumn("Joined", GtkCellRendererText(), Dict([("text", 3)])))
	
	GAccessor.model(gtkbuilder["chat_messages"], GtkTreeModel(message_list_store))
	push!(gtkbuilder["chat_messages"], GtkTreeViewColumn("Author", GtkCellRendererText(), Dict([("text", 1)])))
	push!(gtkbuilder["chat_messages"], GtkTreeViewColumn("Message", GtkCellRendererText(), Dict([("text", 2)])))
	
	GAccessor.model(gtkbuilder["direct_messages"], GtkTreeModel(direct_list_store))
	push!(gtkbuilder["direct_messages"], GtkTreeViewColumn("From", GtkCellRendererText(), Dict([("text", 1)])))
	push!(gtkbuilder["direct_messages"], GtkTreeViewColumn("To", GtkCellRendererText(), Dict([("text", 3)])))
	push!(gtkbuilder["direct_messages"], GtkTreeViewColumn("Message", GtkCellRendererText(), Dict([("text", 4)])))
	
	signal_connect(on_connect_button_clicked, gtkbuilder["connect_button"], :clicked)
	signal_connect(on_refresh_button_clicked, gtkbuilder["refresh_button"], :clicked)
	signal_connect(on_create_chat_button_clicked, gtkbuilder["create_chat_button"], :clicked)
	signal_connect(on_chat_join_clicked, gtkbuilder["chat_join"], :clicked)
	signal_connect(on_send_message, gtkbuilder["message_send"], :activate)
	signal_connect(on_send_message, gtkbuilder["message_send"], :icon_press)
	
	GAccessor.model(gtkbuilder["user_search_completion"], GtkTreeModel(user_list_store))
	
	@info "Showing window"
	showall(window)
	
	@async Gtk.main()
	Gtk.waitforsignal(window, :destroy)
	
	conn === nothing || close(conn)
end

as_bytes(x:: Integer) = reinterpret(UInt8, [x])
as_bytes(x:: String) = transcode(UInt8, x)
bytes2u16(bytes:: Vector{UInt8}) = reinterpret(UInt16, bytes)[1]

set_status(status) = set_gtk_property!(gtkbuilder["status_label"], :label, status)
set_status_err(error) = set_gtk_property!(gtkbuilder["status_label"], :label, "Error: $error")
set_status_err() = set_gtk_property!(gtkbuilder["status_label"], :label, "Unknown error")
set_status_fu() = set_gtk_property!(gtkbuilder["status_label"], :label, "Error: The server hates us")


function on_chat_join_clicked(btn)
	try
		pass = get_gtk_property(gtkbuilder["password_field"], :text, String)
		chatsel = GAccessor.selection(gtkbuilder["chat_list"])
		
		if !hasselection(chatsel)
			@error "No chat selected"
			set_status_err("No chat selected")
			return
		end
		
		chat = chat_list_store[selected(chatsel)]
		chatid = UInt16(chat[1])
		isjoined = chat[4]
		
		if isjoined
			global current_chat = chatid
			repopulate_message_list()
		else
			if isempty(pass)
				req = vcat(UInt8[OP_JOIN_OPEN_CHAT], as_bytes(hton(chatid)))
				write(conn, req)
				bytes = readavailable(conn)
				status = bytes[2]
				
				if status == STAT_FU
					set_status_fu()
				elseif status == STAT_JOIN_OPEN_CHAT_BAD_CHAT
					set_status_err("Bad chat")
				elseif status == STAT_JOIN_OPEN_CHAT_ALREADY_JOINED
					set_status_err("Already joined")
				elseif status == STAT_JOIN_OPEN_CHAT_NOT_OPEN
					set_status_err("Chat not open")
				else
					set_status("Joined chat")
					chat_list_store[selected(chatsel), 4] = true
					chat_messages[chatid] = Message[]
					@info "Joined chat $chat"
				end
			else
				passlenbytes = as_bytes(hton(UInt16(length(pass))))
				req = vcat(UInt8[OP_JOIN_PASSWORD_CHAT], as_bytes(hton(chatid)), passlenbytes, as_bytes(pass))
				write(conn, req)
				bytes = readavailable(conn)
				status = bytes[2]
				
				if status == STAT_FU
					set_status_fu()
				elseif status == STAT_JOIN_PASSWORD_CHAT_BAD_CHAT
					set_status_err("Bad chat")
				elseif status == STAT_JOIN_PASSWORD_CHAT_ALREADY_JOINED
					set_status_err("Already joined")
				elseif status == STAT_JOIN_PASSWORD_CHAT_NOT_PASSWORD
					set_status_err("Chat not password-protected")
				elseif status == STAT_JOIN_PASSWORD_CHAT_BAD_PASSWORD
					set_status_err("Bad password")
				else
					set_status("Joined chat")
					chat_list_store[selected(chatsel), 4] = true
					chat_messages[chatid] = Message[]
					@info "Joined chat $chat"
				end
			end
		end
	catch e
		@error e
		set_status_err()
	end
end

function on_connect_button_clicked(btn)
	if conn === nothing
		try
			@info "Connecting"
			
			ip = parse(IPAddr, get_gtk_property(gtkbuilder["ip_entry"], :text, String))
			port = parse(Int, get_gtk_property(gtkbuilder["port_entry"], :text, String))
			username = get_gtk_property(gtkbuilder["username_entry"], :text, String)
			
			@info "Connecting to $ip:$port as $username"
			
			len = as_bytes(hton(convert(UInt16, length(username))))
			req = vcat(UInt8[OP_HELLO], len, as_bytes(username))
			@info req
			global conn = connect(ip, port)
			write(conn, req)
			bytes = readavailable(conn)
			status = bytes[2]
			
			if status == STAT_OK
				set_gtk_property!(btn, :label, "Disconnect")
				update_chat_list() || return
				update_user_list() || return
				global ouruserid = ntoh(bytes2u16(bytes[3:4]))
				global ourusername = username
				set_status("Connected")
			elseif status == STAT_FU
				set_status_fu()
			elseif status == STAT_HELLO_BAD_NAME
				set_status_err("Bad name")
			elseif status == STAT_HELLO_NAME_TO_LONG
				set_status_err("Name too long")
			end
			
			@info "Received: $bytes"
		catch e
			set_status_err()
			@error e
		end
	else
		@info "Disconnecting"
		close(conn)
		global conn = nothing
		set_gtk_property!(btn, :label, "Connect")
		set_status("Disconnected")
		global current_chat = nothing
		empty!.((chat_messages, user_list_store, message_list_store, chat_list_store))
		@info "Disconnected"
	end
end

function on_create_chat_button_clicked(btn)
	try
		name = get_gtk_property(gtkbuilder["chat_name_entry"], :text, String)
		pass = get_gtk_property(gtkbuilder["chat_password_entry"], :text, String)
		namelenbytes = as_bytes(hton(convert(UInt16, length(name))))
		passlenbytes = as_bytes(hton(convert(UInt16, length(pass))))
		
		if isempty(pass)
			req = vcat(UInt8[OP_CREATE_OPEN_CHAT], namelenbytes, as_bytes(name))
			write(conn, req)
			bytes = readavailable(conn)
			status = bytes[2]
			
			if status == STAT_FU
				set_status_fu()
				return
			elseif status == STAT_CREATE_OPEN_CHAT_BAD_NAME
				set_status_err("Bad chat name")
				return
			elseif status == STAT_CREATE_OPEN_CHAT_TOO_MANY
				set_status_err("Too many chats")
				return
			end
		else
			req = vcat(UInt8[OP_CREATE_PASSWORD_CHAT], namelenbytes, passlenbytes, as_bytes(name), as_bytes(pass))
			write(conn, req)
			bytes = readavailable(conn)
			status = bytes[2]
			
			if status == STAT_FU
				set_status_fu()
				return
			elseif status == STAT_CREATE_PASSWORD_CHAT_BAD_NAME
				set_status_err("Bad chat name")
				return
			elseif status == STAT_CREATE_PASSWORD_CHAT_TOO_MANY
				set_status_err("Too many chats")
				return
			end
		end
		
		id = ntoh(bytes2u16(bytes[3:4]))
		
		update_chat_list() && set_status("Chat created (ID $id)")
	catch e
		@error e
	end
end

function on_refresh_button_clicked(btn)
	try
		update_chat_list() || return
		update_user_list() || return
		set_status("Lists refreshed")
	catch e
		@error e
		set_status_err()
	end
end

function repopulate_message_list()
	empty!(message_list_store)
	messages:: Vector{Message} = chat_messages[current_chat]
	isempty(messages) || append!(message_list_store, [(msdg.userid, msg.username, msg.msg) for msg in messages])
end

function on_send_message(entry, _...)
	try
		msg = get_gtk_property(entry, :text, String)
		chatid = UInt16(current_chat)
		msglenbytes = (as_bytes ∘ hton ∘ UInt16 ∘ length)(msg)
		
		req = vcat(UInt8[OP_SEND_MESSAGE], as_bytes(hton(chatid)), msglenbytes, as_bytes(msg))
		@info "Sending message to chat $chatid: $req"
		write(conn, req)
		bytes = readavailable(conn)
		status = bytes[2]
		
		if status == STAT_FU
			set_status_fu()
			return
		elseif status == STAT_SEND_MESSAGE_BAD_CHAT
			set_status_err("Bad chat")
			return
		elseif status == STAT_SEND_MESSAGE_NOT_IN_CHAT
			set_status_err("Not in chat")
			return
		end
		
		@info "Sent message to chat $chatid"
		push!(chat_messages[chatid], Message(ouruserid, ourusername, msg))
		push!(message_list_store, (ouruserid, ourusername, msg))
	catch e
		@error e
	end
end

function update_chat_list()
	write(conn, UInt8[OP_GET_CHAT_LIST])
	bytes = readavailable(conn)
	status = bytes[2]
	
	if status == STAT_FU
		set_status_fu()
		return false
	end
	
	chatslen = ntoh(bytes2u16(bytes[3:4]))
	empty!(chat_list_store)
	ind = 5
	for i in 1:chatslen
		id = ntoh(bytes2u16(bytes[ind:ind+1]))
		@info "isopen=$(bytes[ind+2])"
		is_open = bytes[ind+2] != 0
		namelen = ntoh(bytes2u16(bytes[ind+3:ind+4]))
		name = String(bytes[ind+5:ind+5+namelen-1])
		@info "Chat #$id: $name (len $namelen); open: $is_open"
		push!(chat_list_store, (id, is_open, name))
		ind += 5 + namelen
	end
	
	return true
end

function update_user_list()
	write(conn, UInt8[OP_GET_USER_LIST])
	bytes = readavailable(conn)
	status = bytes[2]
	
	if status == STAT_FU
		set_status_fu()
		return false
	end
	
	userslen = ntoh(bytes2u16(bytes[3:4]))
	empty!(user_list_store)
	ind = 5
	for i in 1:userslen
		id = ntoh(bytes2u16(bytes[ind:ind+1]))
		namelen = ntoh(bytes2u16(bytes[ind+2:ind+3]))
		name = String(bytes[ind+4:ind+4+namelen-1])
		@info "User #$id: $name (len $namelen)"
		push!(user_list_store, (id, name))
		ind += 4 + namelen
	end
	
	return true
end

end # module
