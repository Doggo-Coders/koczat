# Koczat
TCP chat. Server written in C (cross-platform) and the client is in Julia and Gtk.jl.
That's enough network programming for the rest of the year I'm pretty sure. This was my (MasFlam's)
most annoying and infuriating project yet. A tough record to break...

## Running:
### Server
#### Linux
Compile with `cc -o server server/server.c`
#### Windows
Compile with `cc -o server server/server.c -lWs2_32`

*Note from TFKls - The WinSocks were tested but they still may break.
Please take care of this
diving in too deep into this WinAPI crossplatform compat (for now :O).*
### Client
#### Linux
Run with:
```
julia --project
Pkg> instantiate # install dependencies
julia> include("src/KoczatClient.jl")
julia> KoczatClient.julia_main()
```
#### Windows
It should work in the same way but our tests have showed us that some weird errors may appear, as such we do not
actively guarantee that the client works crossplatform.

## Have fun never using this chat!
