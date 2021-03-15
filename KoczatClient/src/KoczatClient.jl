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
end

end # module
