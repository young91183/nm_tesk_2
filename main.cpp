#include "./Server/Server.h"
#include "./Log_To_DB/Log_To_DB.h"

//  main 함수
int main() {
		MysqlPool mysqlPool = createMysqlPool(); 
    	int i;
		const char* mysql_server = "localhost";
		const char* user = "root"; 
		const char* password = "0000"; 
		const char* database = "client_management"; // DB
	
    	for(i=0 ; i<10 ; i++){ // 초기 connection 개수 설정
    		MYSQL *conn = mysql_init(NULL); // Mysql Connection 생성 후 기본 값 초기화
    		// mysql 설정 정보를 가지고 Connection 생성
    	 	if (!mysql_real_connect(conn, mysql_server, user, password, database, 0, NULL, 0)) { 
				std::cerr << mysql_error(conn) << std::endl;
				exit(1);
            }
			insertNodeAtBack(&mysqlPool, conn);
    	}

    	Server* server = new Server(&mysqlPool); // Server Class 생성
		if(!(write_log_db(&mysqlPool, "Server : Server_ON", "_server"))) return 0;
	    server->request_file();
		delete server;
    	return 0;
}
