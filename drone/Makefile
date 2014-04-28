#
# Simple .so Makefile
#
 
CC      := gcc
LD      := ld
CFLAGS  := -W -Wall -fPIC
DEBUG   := #-DDEBUG -g
OPENSSL := -DQueenSSL -DQueenSSL_NonBlock
LDFLAGS := -shared
SOURCE  := $(wildcard *.c)
OBJS    := $(patsubst %.c,%.o,$(SOURCE))
TARGET_LIB := drone.so
LIB     :=  -lpthread -lm -lssl -lcrypto


all:$(OBJS)
	echo $(OBJS)
	$(CC) $(LDFLAGS) $(LIB) -o $(TARGET_LIB) $(OBJS) 
 
%.o:%.c
	@echo Compiling $< ...
	$(CC) -c $(CFLAGS) $(DEBUG) $(OPENSSL)  $< -o $*.o
 
.PHONY: clean
 
clean:
	rm *.so *.o -rf