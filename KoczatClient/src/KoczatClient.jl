module KoczatClient

using Gtk, Sockets

const glade_xml = read(joinpath(@__DIR__, "../chatapp.glade"), String)

julia_main() = (main(); Cint(0))

function main(args = ARGS)
	global gtkbuilder = GtkBuilder(buffer = glade_xml)
	global window = gtkbuilder["application"]
	
	signal_connect(on_connect_button_clicked, gtkbuilder["connect_button"], :clicked)
	
	showall(window)
end

function on_connect_button_clicked(btn)
	println("[klik klik]")
	ip = parse(IPAddr, get_gtk_property(gtkbuilder["ip_entry"], :text, String))
	port = parse(Int, get_gtk_property(gtkbuilder["port_entry"], :text, String))
	username = get_gtk_property(gtkbuilder["username_entry"], :text, String)
	
	println("[] $ip:$port $username")
	
	global conn = connect(ip, port)
	write(conn, b"\x01\x00\x06abcdef")
	bytes = readavailable(conn)
	
	println("Received: $bytes")
end

end # module
