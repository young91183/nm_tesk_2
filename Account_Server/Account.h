#ifndef ACCOUNT_H
#define ACCOUNT_H

#include "../PC_Info_Recv/Save_Server.h"
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


#define MAX_CONNECTIONS 500 
using namespace nlohmann;

std::string sha512(const std::string& data);
std::string base64(const unsigned char* buffer, size_t length);

class Account_Session {
public:
	explicit Account_Session(int socket);
	nlohmann::json start();
private:
	int client_socket;
	nlohmann::json login_id;
    void read_();
	std::string login(json account, MYSQL *conn);
	std::string logout(json account, MYSQL *conn);
	std::string join(json account, MYSQL *conn);
};


#endif // ACCOUNT_H
