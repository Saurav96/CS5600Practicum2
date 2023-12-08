all: ./server/server ./client/rfs
	echo "MAKE: Building all"

server/server: ./server/server.c ./util/util.h
	echo "MAKE: Building Server"
	gcc -Wall ./server/server.c -o ./server/server

client/rfs: ./client/client.c ./util/util.h
	echo "MAKE: Building Client"
	gcc -Wall ./client/client.c -o ./client/rfs

clean:
	rm -f ./server/server ./client/rfs