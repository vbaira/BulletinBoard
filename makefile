OBJS1 =  board_client.o  board_server.o  channel.o
OBJS2 =  boardpost.o
SOURCE1 = ./client-server/board_client.c ./client-server/board_server.c  ./client-server/channel.c
SOURCE2 = ./boardpost-app/boardpost.c
OUT1 = board
OUT2 = boardpost
CC = gcc
FLAGS = -g -c -lpthread



all: $(OUT1) $(OUT2)


$(OUT1): $(OBJS1)
	$(CC) -g $(OBJS1) -o $(OUT1)


board_client.o: ./client-server/board_client.c
	$(CC) $(FLAGS)  ./client-server/board_client.c


board_server.o: ./client-server/board_server.c
	$(CC) $(FLAGS)  ./client-server/board_server.c


channel.o: ./client-server/channel.c
	$(CC) $(FLAGS)  ./client-server/channel.c	



$(OUT2): $(OBJS2)
	$(CC) -g $(OBJS2) -o $(OUT2)


boardpost.o: ./boardpost-app/boardpost.c
	$(CC) $(FLAGS) ./boardpost-app/boardpost.c
