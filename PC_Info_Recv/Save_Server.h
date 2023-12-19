#ifndef SAVE_SERVER_H
#define SAVE_SERVER_H

#include "../Pool/Pool.h"
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>
#include <string>
#include <iostream>
#include <cstring> // for memset, communication
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> // for read
#include <thread>
#define MAX_CONNECTIONS 500 
using namespace nlohmann;


class PC_Info_Session {
public:
    explicit PC_Info_Session(int socket, MysqlPool* pool);
    void start();
private:
    int client_socket;
    MysqlPool* mysqlPool;
    void read_();
};

/*
class Server {
private:
    int server_socket;
    MysqlPool* mysqlPool;
    ThreadPool* threadPool;
    fd_set master_set;
    int max_sd;
public:
    ~Server();
    Server(MysqlPool* pool);
    void start();
};
*/

void saveDB(json pc_data, MysqlPool *mysqlPool);
int save_cpu_info(json j, MYSQL * conn, std::string id, std::string up_time) ;
int save_memory_info(json j, MYSQL * conn, std::string id, std::string up_time) ;
int save_disk_info(json j, MYSQL * conn, std::string id, std::string up_time);
int save_nic_info(json nic, MYSQL * conn, std::string id, std::string up_time) ;
int save_account_info(json j, MYSQL * conn, std::string id, std::string up_time) ;

int check_login_state(std::string id, MYSQL * conn);

#endif // SAVE_SERVER_H
