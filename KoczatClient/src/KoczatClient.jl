module KoczatClient

using Gtk, Sockets, Logging, Base.Threads

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
event_loop_task = nothing
continue_event_loop = nothing
awaited_packet_channel = nothing
forced_disconnected = nothing
user_joined_chat = nothing
received_message = nothing
received_direct = nothing
user_joined_chat_lock = nothing
received_message_lock = nothing
received_direct_lock = nothing

julia_main() = (main(); Cint(0))
function main(args = ARGS)
	global gtkbuilder = GtkBuilder(buffer = glade_xml)
	global window = gtkbuilder["application"]
	global user_list_store = GtkListStore(UInt16, String)
	global chat_list_store = GtkListStore(UInt16, Bool, String, Bool)
	global message_list_store = GtkListStore(UInt16, String, String)
	global chat_messages = Dict{UInt16, Vector{Message}}()
	global direct_list_store = GtkListStore(UInt16, String, UInt16, String, String)
	global continue_event_loop = Atomic{Bool}(false)
	global awaited_packet_channel = Channel{Vector{UInt8}}()
	global forced_disconnected = Atomic{Bool}(false)
	global user_joined_chat_lock = ReentrantLock()
	global received_message_lock = ReentrantLock()
	global received_direct_lock = ReentrantLock()
	
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
	signal_connect(on_send_direct, gtkbuilder["direct_send"], :activate)
	signal_connect(on_send_direct, gtkbuilder["direct_send"], :icon_press)
	
	GAccessor.model(gtkbuilder["user_search_completion"], GtkTreeModel(user_list_store))
	
	@info "Showing window"
	showall(window)
	
	g_timeout_add(update_from_events, 20)
	
	@async Gtk.main()
	Gtk.waitforsignal(window, :destroy)
	
	continue_event_loop[] = false
	wait(event_loop_task)
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
				bytes = await_packet()
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
				bytes = await_packet()
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
			Sockets.nagle(conn, false)
			Sockets.quickack(conn, false)
			write(conn, req)
			bytes = readavailable(conn)
			status = bytes[2]
			
			if status == STAT_OK
				set_gtk_property!(btn, :label, "Disconnect")
				global ouruserid = ntoh(bytes2u16(bytes[3:4]))
				global ourusername = username
				continue_event_loop[] = true
				global event_loop_task = Threads.@spawn event_loop_fn()
				update_chat_list() || return
				update_user_list() || return
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
		continue_event_loop[] = false
		wait(event_loop_task)
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
			bytes = await_packet()
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
			bytes = await_packet()
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
	for msg in messages
		push!(message_list_store, (msg.userid, msg.username, msg.msg))
	end
end

function on_send_direct(entry, _...)
	try
		msg = get_gtk_property(entry, :text, String)
		usersel = GAccessor.selection(gtkbuilder["user_list"])
		
		if !hasselection(usersel)
			set_status_err("No user selected")
			return
		end
		
		userid, username = user_list_store[selected(usersel)]
		msglenbytes = (as_bytes ∘ hton ∘ UInt16 ∘ length)(msg)
		
		req = vcat(UInt8[OP_SEND_DIRECT], as_bytes(hton(UInt16(userid))), msglenbytes, as_bytes(msg))
		@info "RAWDATAAAA $req"
		write(conn, req)
		bytes = await_packet()
		status = bytes[2]
		
		if status == STAT_FU
			set_status_fu()
			return
		elseif status == STAT_SEND_DIRECT_BAD_USER
			set_status_err("Bad user")
			return
		end
		
		@info "Sent message to user #$userid $username"
		push!(direct_list_store, (ouruserid, ourusername, userid, username, msg))
	catch e
		@error e
	end
end

function on_send_message(entry, _...)
	try
		msg = get_gtk_property(entry, :text, String)
		chatid = UInt16(current_chat)
		msglenbytes = (as_bytes ∘ hton ∘ UInt16 ∘ length)(msg)
		
		req = vcat(UInt8[OP_SEND_MESSAGE], as_bytes(hton(chatid)), msglenbytes, as_bytes(msg))
		@info "Sending message to chat $chatid: $req"
		write(conn, req)
		bytes = await_packet()
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
	bytes = await_packet()
	status = bytes[2]
	
	if status == STAT_FU
		set_status_fu()
		return false
	end
	
	chatslen = ntoh(bytes2u16(bytes[3:4]))

  joined = Dict{UInt16, Bool}()
  
    for i in 1:256
        try
            id = chat_list_store[i, 1]
            val = chat_list_store[i, 4]
            joined[id] = val 
        catch
            break
        end
    end
	empty!(chat_list_store)
	ind = 5
	for i in 1:chatslen
		id = ntoh(bytes2u16(bytes[ind:ind+1]))
		@info "isopen=$(bytes[ind+2])"
		is_open = bytes[ind+2] != 0
		namelen = ntoh(bytes2u16(bytes[ind+3:ind+4]))
		name = String(bytes[ind+5:ind+5+namelen-1])
		@info "Chat #$id: $name (len $namelen); open: $is_open"
      is_joined = false
      try
          is_joined = joined[id]
      catch
          is_joined = false
      end
		push!(chat_list_store, (id, is_open, name, is_joined))
		ind += 5 + namelen
	end
	
	true
end

function update_user_list()
	write(conn, UInt8[OP_GET_USER_LIST])
	bytes = await_packet()
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
	
	true
end

function await_packet():: Vector{UInt8}
	take!(awaited_packet_channel)
end

function update_from_events()
	if forced_disconnected[]
		set_gtk_property!(gtkbuilder["connect_button"], :label, "Disconnect")
		global current_chat = nothing
		empty!.((chat_messages, user_list_store, message_list_store, chat_list_store))
		forced_disconnected[] = false
	end
	lock(user_joined_chat_lock)
	if user_joined_chat !== nothing
		chatid, userid = user_joined_chat
		@info "User #$userid joined chat #$chatid"
		global user_joined_chat = nothing
	end
	unlock(user_joined_chat_lock)
	lock(received_message_lock)
	if received_message !== nothing
		chatid, userid, msg = received_message
		@info "Received message by user #$userid in chat #$chatid: $msg"
		username = ""
		for i in 1:256
			u = user_list_store[i]
			if u[1] == userid
				username = u[2]
				break
			end
		end
		push!(chat_messages[current_chat], Message(userid, username, msg))
		repopulate_message_list()
		global received_message = nothing
	end
	unlock(received_message_lock)
	lock(received_direct_lock)
	if received_direct !== nothing
		userid, msg = received_direct
		@info "Received direct message from user #$userid: $msg"
		username = ""
		for i in 1:256
			u = user_list_store[i]
			if u[1] == userid
				username = u[2]
				break
			end
		end
		push!(direct_list_store, (userid, username, ouruserid, ourusername, msg))
		global received_direct = nothing
	end
	unlock(received_direct_lock)
	Cint(true)
end

function event_loop_fn()
	try
		while continue_event_loop[]
			bytes = readavailable(conn)
			
			if isempty(bytes)
				@error "We received empty bytes in the event loop. This is not good!"
				continue
			end
			
			@info "In event loop we just got: $bytes"
			
			op = bytes[1]
			if op == OP_FORCED_DISCONNECT
				global conn = nothing
				continue_event_loop[] = false
				forced_disconnected[] = true
			elseif op == OP_USER_JOINED_CHAT
				lock(user_joined_chat_lock)
				chatid = ntoh(bytes2u16(bytes[2:3]))
				userid = ntoh(bytes2u16(bytes[4:5]))
				global user_joined_chat = (chatid, userid)
				unlock(user_joined_chat_lock)
			elseif op == OP_RECEIVE_MESSAGE
				lock(received_message_lock)
				chatid = ntoh(bytes2u16(bytes[2:3]))
				userid = ntoh(bytes2u16(bytes[4:5]))
				msglen = ntoh(bytes2u16(bytes[6:7]))
				msg = String(bytes[8:8+msglen-1])
				global received_message = (chatid, userid, msg)
				unlock(received_message_lock)
			elseif op == OP_RECEIVE_DIRECT
				lock(received_direct_lock)
				userid = ntoh(bytes2u16(bytes[2:3]))
				msglen = ntoh(bytes2u16(bytes[4:5]))
				msg = String(bytes[6:6+msglen-1])
				global received_direct = (userid, msg)
				unlock(received_direct_lock)
			elseif op in (
				OP_GET_USER_LIST_RESP, OP_GET_CHAT_LIST_RESP, OP_CREATE_OPEN_CHAT_RESP,
				OP_CREATE_PASSWORD_CHAT_RESP, OP_JOIN_OPEN_CHAT_RESP,
				OP_JOIN_PASSWORD_CHAT_RESP, OP_SEND_MESSAGE_RESP, OP_SEND_DIRECT_RESP
			)
				put!(awaited_packet_channel, bytes)
			else
				@error "Server sent invalid opcode: $op. That's not good!"
			end
		end
		@info "Event loop is ovah!"
	catch e
		@error "Error in event loop"
		@error e
	end
end

end # module
