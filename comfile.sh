#!/bin/bash

# 각 명령을 순차적으로 실행합니다.
g++ -c -std=c++17 ./Account_Server/Account.cpp
g++ -c ./Pool/Pool.cpp
g++ -c -std=c++17 ./PC_Info_Recv/Save_Server.cpp
g++ -c ./Server/Server.cpp
g++ -c ./File_Recv/File_Recv.cpp
g++ -c ./Log_To_DB/Log_To_DB.cpp
g++ -c main.cpp
g++ Account.o Pool.o Server.o Save_Server.o File_Recv.o main.o Log_To_DB.o -o qwe -lmysqlclient -lpthread -lcrypto

rm Account.o Pool.o Server.o Save_Server.o File_Recv.o main.o Log_To_DB.o

# 모든 명령이 정상적으로 실행되었는지 확인합니다.
if [ $? -eq 0 ]
then
  echo "모든 컴파일이 성공적으로 완료되었습니다."
else
  echo "에러가 발생했습니다. 로그를 확인해주세요."
fi
