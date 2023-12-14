#ifndef ACCOUNT_SERVER_H
#define ACCOUNT_SERVER_H

#include "../Pool/Pool.h"
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

#define MAX_CONNECTIONS 500 
using namespace nlohmann;


class Account_Session {
public:
    	explicit Account_Session(int socket);
    	void start();
private:
	int client_socket;
    	void read_();
	std::string login(json account, MYSQL *conn);
	std::string logout(json account, MYSQL *conn);
	std::string join(json account, MYSQL *conn);
};

class Account_Server{
private:
    int server_socket;
    ThreadPool* threadPool;

public:
    ~Account_Server();
    Account_Server();
    void start(int server_socket);
};

#endif // ACCOUNT_SERVER_H
