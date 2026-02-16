# retchat

> app de chat TCP/IP MUUUUY simple de terminal hecha en C, que te permite comunicarte con otros clientes conectados a la misma red

## uso

### descargar

1. `git clone https://github.com/retuci0/retchat`
2. `cd retchat`

### compilar

#### linux
`make clean all`

#### windows
`.\win\compile_MINGW.bat` o `.\win\compile_MSVC.bat`

### ejecutar

#### linux
- server: `./bin/server`
- cliente(s): `./bin/client <ip del server>`

#### windows
- server: abrir `bin/server.exe`
- cliente: abrir `bin/client.exe` (o desde la terminal para especificar IP del server)

### comandos

`/nick <nombre>`: cambiar de nombre de usuario
`/join <sala>`: cambiar de sala