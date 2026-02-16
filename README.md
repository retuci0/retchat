# retchat

> app de chat TCP/IP MUUUUY simple de terminal hecha en C, que te permite comunicarte con otros clientes conectados a la misma red, mediante salas

## uso

### descargar

1. `git clone https://github.com/retuci0/retchat`
2. `cd retchat`

### compilar

#### linux
1. `make clean all`

#### windows
1. `cd win`
2. `.\compile_MINGW.bat` o `.\compile_MSVC.bat`

### ejecutar

> los argumentos son opcionales,
> - la ip por defecto es localhost (127.0.0.1)
> - el puerto por defecto es 6677 (SIX SEVEN)

#### linux
- server: `./bin/server <puerto`
- cliente(s): `./bin/client <ip local del server> <puerto>`

#### windows
- server: `.\bin\server.exe <puerto>`
- cliente(s): `.\bin\client.exe <ip local del server> <puerto>`

### comandos

`/nick <nombre>`: cambiar de nombre de usuario
`/join <sala>`: cambiar de sala