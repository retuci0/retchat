cd ..
mkdir bin
gcc -pthread server.c -o bin/server.exe -lws2_32
gcc -pthread client.c -o bin/client.exe -lws2_32