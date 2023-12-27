CC = g++
CFLAGS = -c -std=c++17
LFLAGS = -lmysqlclient -lpthread -lcrypto
OBJECTS = Account.o Pool.o Server.o Save_Server.o File_Recv.o Log_To_DB.o main.o

on_server:$(OBJECTS)
	$(CC) $(OBJECTS) -o on_server $(LFLAGS)

Account.o : Account_Server/Account.cpp
	$(CC) $(CFLAGS) Account_Server/Account.cpp

Pool.o : Pool/Pool.cpp
	$(CC) $(CFLAGS) Pool/Pool.cpp

Save_Server.o : PC_Info_Recv/Save_Server.cpp
	$(CC) $(CFLAGS) PC_Info_Recv/Save_Server.cpp

Server.o : Server/Server.cpp
	$(CC) $(CFLAGS) Server/Server.cpp

File_Recv.o : File_Recv/File_Recv.cpp
	$(CC) $(CFLAGS) File_Recv/File_Recv.cpp

Log_To_DB.o : Log_To_DB/Log_To_DB.cpp
	$(CC) $(CFLAGS) Log_To_DB/Log_To_DB.cpp

main.o : main.cpp
	$(CC) $(CFLAGS) main.cpp

clean :
	rm -rf *o

