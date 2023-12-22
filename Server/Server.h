#ifndef SERVER_H
#define SERVER_H

#include "../Pool/Pool.h"
#include "../Log_To_DB/Log_To_DB.h"
#include "../PC_Info_Recv/Save_Server.h"
#include "../Account_Server/Account.h"
#include "../File_Recv/File_Recv.h"
#include <nlohmann/json.hpp>
#include <mysql/mysql.h>
#include <string>
#include <iostream>
#include <cstring> // for memset
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> // for read
#include <thread>

#include <openssl/sha.h> // SHA 관련 함수를 사용하기 위한 헤더파일
#include <openssl/bio.h> // BIO 관련 함수를 사용하기 위한 헤더파일
#include <openssl/evp.h> // EVP 관련 함수를 사용하기 위한 헤더파일

class Server{
private:
    // 멤버변수
    bool isLoop;
    int server_socket, max_sd;
    MysqlPool* mysqlPool;
    ThreadPool* threadPool;
    fd_set master_set;
    std::mutex mtx_server;
    nlohmann::json client_socket_json;

public:
    ~Server();
    Server(MysqlPool* pool);
    void ac_start();
    void request_file();
    // void ac_start_wrapper(int i);
};

#endif // SERVER_H
