ROOT_PATH=$(shell pwd)
INCLUDE=-I$(ROOT_PATH)/mysql-lib/include
LIB_PATH=-L$(ROOT_PATH)/mysql-lib/lib
#LDFLAGS=-lmysqlclient
FLAGS=-lpthread

server:sever1.cpp
	g++ -o $@ $^ $(INCLUDE) $(LIB_PATH) $(FLAGS)  

.PHONY:clean
clean:	
	rm -rf server
