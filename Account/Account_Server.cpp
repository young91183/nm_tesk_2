#include "Account_Server.h"

extern std::mutex mtx;



// 계정인증 절차 구현 부
Account_Session::Account_Session(int socket) : client_socket(socket) {}


void Account_Session::start() {
	read_();
}

// 계정인증 정보 수신 + 결과 반환
void Account_Session::read_() {
		std::string total_buffer, result;
		char buffer[500];
		ssize_t n;
		MYSQL *conn;
		const char* server = "localhost";
		const char* user = "root";
		const char* password = "0000";
		const char* database = "client_management";
		
		conn = mysql_init(NULL);
		if(!mysql_real_connect(conn, srever, user, password, database, 0, NULL, 0)) {
			std::cerr << "mysql database err" << mysql_error(conn))<< std::endl;
			exit(1)
		}
		
		while((n = read(client_socket, buffer, sizeof(buffer)-1)) > 0) { 
			total_buffer += buffer; 
			memset(buffer, 0 , sizeof(buffer)); // 버퍼 초기화
		}
		if (n < 0){ 
			std::perror("read");
			exit(1);
            	} 
		nlohmann::json j = nlohmann::json::parse(total_buffer); // dump된 메세지를 json 객체로 복구
            	
		if (j["request"] == "login") result = login(j);
		else if (j["request"] == "logout") result = logout(j);
		else if (j["request"] == "join") result = join(j);
		else result = "잘못된 요청입니다.\n"
		//write result 진행
		
		close(client_socket); // 소켓 종료
		mysql_close(conn);
}

// 로그인 구현 부
std::string Account_Session::login(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result = "ok", query, id, pw, up_time, ip;
	
	id = account["id"].get<std::string>();
	pw = account["password"].get<std::string>();
	up_time = account["up_time"].get<std::string>();
	ip = account["ip"].get<std::string>();		

	// 아이디 비밀번호 조회
	std::string query = "SELECT state FROM account_info WHERE id='" + id + "' AND password='" + pw + "';";

	if(mysql_query(conn, query.c_str())) {
		std::cerr << mysql_error(conn) <<std::endl;
		res = mysql_use_result(conn);
		mysql_free_result(res);
		return mysql_error(conn);
	}

	res = mysql_use_result(conn);
	std::string state;
	if ((row = mysql_fetch_row(res)) != NULL) {
		state = std::string(row[0]);
	} else { // 비밀번호 아이디가 일치하지 않을 때
		std::cout << "아이디 또는 비밀번호가 일치하지 않습니다.\n";
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "wrong_account_data";
		return result;
	}

	if (state == "login") { // 해당 아이디가 이미 login 중인 경우
		std::cout << "다른 환경에서 이미 로그인 중입니다.\n";
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "d_pc";
		return result;
	} else if (state == "logout" && state !="") { // 해당 아이디가 로그인 가능할 때
		query = "UPDATE account_info SET state='login', ip='" + ip +  "up_time='" + up_time + "' WHERE id='" + id + "';";
		if(mysql_query(conn, query.c_str())) {
			std::cerr << mysql_error(conn) <<std::endl;
			res = mysql_use_result(conn);
			mysql_free_result(res);
			result = "login_DB업데이트_실패";
			return result;
		}
	}
	
	mysql_free_result(res);
	return result;
}

// 로그아웃 구현 부
std::string Account_Session::logout(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result = "ok", query, id, pw, up_time, ip;
	
	id = account["id"].get<std::string>();
	pw = account["password"].get<std::string>();
	up_time = account["up_time"].get<std::string>();
	ip = account["ip"].get<std::string>();

	// 아이디 비밀번호 조회
	std::string query = "SELECT state FROM account_info WHERE id='" + id + "' AND password='" + pw + "';";

	if(mysql_query(conn, query.c_str())) {
		std::cerr << mysql_error(conn) <<std::endl;
		res = mysql_use_result(conn);
		mysql_free_result(res);
		return mysql_error(conn);
	}

	// 로그아웃 전 계정정보 Check
	std::string state;
	if ((row = mysql_fetch_row(res)) != NULL) {
		state = std::string(row[0]);
	} else { // 비밀번호 아이디가 일치하지 않을 때
		std::cout << "아이디 또는 비밀번호가 일치하지 않습니다.\n";
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "wrong_account_data";
		return result;
	}

	// 로그아웃 DB 업데이트
	if (state == "logout") { // 해당 아이디가 이미 logout 상태인 경우
		std::cout << "이미 로그아웃 된 계정\n";
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "Account already logout";
		return result;
	} else if (state == "login" && state !="") { // 해당 아이디가 로그인 중일 때
		query = "UPDATE account_info SET state='logout', ip='" + ip +  "up_time='" + up_time + "' WHERE id='" + id + "';";
		if(mysql_query(conn, query.c_str())) {
			std::cerr << mysql_error(conn) <<std::endl;
			res = mysql_use_result(conn);
			mysql_free_result(res);
			result = "logout_DB업데이트_실패";
			return result;
		}
	}

	res = mysql_use_result(conn);
	mysql_free_result(res);
	return result;
}


// 회원가입 구현 부
std::string Account_Session::join(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result = "ok", id, pw, up_time, ip;
	std::string q_key, query = "", q_value;
	
	id = account["id"].get<std::string>();
	pw = account["password"].get<std::string>();
	up_time = account["up_time"].get<std::string>();
	ip = account["ip"].get<std::string>();	

	
	
	query = "INSERT INTO account_info ";
	q_key = "(state, ";
	q_value = ") VALUES ('logout', ";
	
	for (auto& [key, value] : account[i].items()) {
		if(key == "request") continue;
		q_key += key + ", ";
		q_value += "'" + value + "', ";
	}
	q_key = q_key.substr(0, q_key.size()-2);
	q_value = q_value.substr(0, q_value.size()-2);
	q_value += ")";

	query += q_key + q_value;

	if(mysql_query(conn, query.c_str())) {
		std::cerr << mysql_error(conn) <<std::endl;
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "이미 있는 계정이거나 잘못된 정보입니다.\n";
		return result;
	}

	res = mysql_use_result(conn);
	mysql_free_result(res);
	return result;
}



// 계정인증 통신 시작 부 (class Server)

Account_Server::~Account_Server() {
	threadPool->join();
	delete threadPool;
}


Account_Server::Account_Server() {
	threadPool = new ThreadPool(5); // 5개의 ThreadPool 생성
	int i = 0, port =8081;
	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
		std::perror("socket");
		exit(1);
	}

	sockaddr_in server_address{};
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
		std::perror("bind");
		exit(1);
	}

	listen(server_socket, MAX_CONNECTIONS);
	std::cout<<"insert serversocket to ThreadPool\n:"; 
	for (i = 0; i < 5; i++) {
		threadPool->enqueue([this, i]() { start(); });
	}
}


void Account_Server::start() {
	std::cout << "start start! \n"; // Check!!!!
    while (true) {
        fd_set copy_set = master_set; // 복사본 생성

        if (select(max_sd + 1, &copy_set, nullptr, nullptr, nullptr) < 0) { // socket 
            std::perror("select");
            exit(1);
        }

        for (int fd = 0; fd <= max_sd; fd++) {
            if (!FD_ISSET(fd, &copy_set)) continue;

            sockaddr_in client_address{};
            socklen_t client_len = sizeof(client_address);
			std::cout << "accept start! \n"; // Check!!!!
            int client_socket = accept(fd, (struct sockaddr*)&client_address, &client_len);
			
            if (client_socket < 0) {
				std::perror("accept");
				break;
			}

            std::cout << "session start! \n";
            Account_Session* session = new Account_Session(client_socket, mysqlPool);
            std::thread([session]() {
                    session->start();
                    delete session;
            }).detach();
        }
    }
}


