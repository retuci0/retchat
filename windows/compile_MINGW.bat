@echo off
echo compilando (MinGW)...

if not exist ..\bin mkdir ..\bin

echo compilando servidor...
gcc -Wall -std=c99 -o ..\bin\server.exe server.c ..\common\crypto.c -lws2_32 -lpthread

echo compilando cliente...
gcc -Wall -std=c99 -o ..\bin\client.exe client.c ..\common\crypto.c -lws2_32 -lpthread

echo listo.
pause
