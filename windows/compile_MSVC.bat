@echo off
echo compilando (MSVC)...

if not exist ..\bin mkdir ..\bin

echo compilando servidor...
cl /nologo /W3 /Fe..\bin\server.exe server.c ..\common\crypto.c /I..\common /link ws2_32.lib

echo compilando cliente...
cl /nologo /W3 /Fe..\bin\client.exe client.c ..\common\crypto.c /I..\common /link ws2_32.lib

echo listo.
pause
