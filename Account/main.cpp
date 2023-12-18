#include "Account_Server.h"

extern std::mutex mtx;
//  main 함수
int main() {
    	
    	Account_Server* account_server = new Account_Server(); // Server Class 생성
	    delete account_server;
    	return 0;
}
