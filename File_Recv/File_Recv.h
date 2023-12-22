#ifndef FILE_RECV_H
#define FILE_RECV_H

#include "../Log_To_DB/Log_To_DB.h"
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <cstring>
#define MAX_CONNECTIONS 500 
using namespace nlohmann;

class File_Recv{
public:
    File_Recv(MysqlPool *pool, int recv_socket, std::string id);
    void start();

private:
    int client_socket;
    MysqlPool* mysqlPool;
    std::string client_id;
};

#endif // FILE_RECV_H
