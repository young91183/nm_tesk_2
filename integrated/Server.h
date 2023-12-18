#ifndef ACCOUNT_SERVER_H
#define ACCOUNT_SERVER_H

#include "Pool.h"
#include "Save_Server.h"
#include "Account.h"
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
    int server_socket;
    MysqlPool* mysqlPool;
    ThreadPool* threadPool;
    fd_set master_set;
    int max_sd;

public:
    ~Server();
    Server(MysqlPool* pool);
    void ac_start();
};



#endif // SERVER_H 
