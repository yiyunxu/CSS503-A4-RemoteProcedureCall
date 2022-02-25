#!/bin/bash

#run this in cli
#chmod a+x ./BUILD


echo "Building client.o and server.o ..."
cd mylib
g++ -c \
    -Wall -Werror -pedantic \
    -O3 \
    client.cc

g++ -c \
    -Wall -Werror -pedantic \
    -O3 \
    server.cc
cd ..


echo
echo "Building client and server exe ..."
g++ -Imylib \
    -Wall -Werror -pedantic \
    clientDriver.cc \
    mylib/client.o \
    -o client

g++ -Imylib \
    -Wall -Werror -pedantic \
    serverDriver.cc \
    mylib/server.o \
    -o server


echo
echo "Running server driver ..." 
./server 8080
