# retchat

> app de chat TCP/IP MUUUUY simple de terminal hecha en C, que te permite comunicarte con otros clientes conectados a la misma red o remotamente, mediante salas. encriptación básica mediante SDHA256, XOR e intercambio de claves Diffie-Hellman.

## uso

### descargar

1. `git clone https://github.com/retuci0/retchat/`
2. `cd retchat`

### compilar

 `make clean all`

### ejecutar

> los argumentos son opcionales,
> - la ip por defecto es localhost (127.0.0.1)
> - el puerto por defecto es 6677 (SIX SEVEN!!!)

- `./bin/server <puerto>`


### comandos

`/nick <nombre>`: cambiar de nombre de usuario
`/join <sala>`: cambiar de sala
