#include "Save_Server.h"


extern std::mutex mtx;

void saveDB(json pc_data, MysqlPool *mysqlPool);

//Session public zone
Session::Session(int socket, MysqlPool* pool) : client_socket(socket), mysqlPool(pool) {}


void Session::start() {
    read_();
}


//Session private zone
void Session::read_() {
    std::string total_buffer;
    char buffer[500];
    ssize_t n;
    std::cout << "read start! \n";
    while((n = read(client_socket, buffer, sizeof(buffer)-1)) > 0) { 
        total_buffer += buffer; 
        memset(buffer, 0 , sizeof(buffer)); // 버퍼 초기화
    }
    if (n < 0){ 
        std::perror("read");
        exit(1);
    }
    nlohmann::json j = nlohmann::json::parse(total_buffer); // dump된 메세지를 json 객체로 복구
    close(client_socket); // 소켓 종료
    std::cout << "save start! \n";
    saveDB(j, mysqlPool); // DB저장
}


Server::~Server() {
	threadPool->join();
	delete threadPool;
}


Server::Server(MysqlPool* pool) : mysqlPool(pool) {
    threadPool = new ThreadPool(10); // 10개의 ThreadPool 생성
    int i, port = 8080;
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
    FD_ZERO(&master_set); 
    FD_SET(server_socket, &master_set); 
    max_sd = server_socket;
    for (i = 0; i < 10; i++) { 
        threadPool->enqueue([this, i]() { start(); });
    }
}


void Server::start() {
	std::cout << "start start!\n"; // Check!!!!
    while (true) {
        fd_set copy_set = master_set; // 복사본 생성

        if (select(max_sd + 1, &copy_set, nullptr, nullptr, nullptr) < 0) {
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
            Session* session = new Session(client_socket, mysqlPool);
            std::thread([session]() {
                    session->start();
                    delete session;
            }).detach();
        }
    }
}


// json 받아서 DB에 저장하는 함수
void saveDB(json pc_data, MysqlPool *mysqlPool) {
    MYSQL *conn = getNodeFromFront(mysqlPool); // Pool에 있는 Connection 불러오기
	std::string id, up_time;
	id = pc_data["account_info"]["id"].get<std::string>();
	up_time = pc_data["account_info"]["up_time"].get<std::string>();

    std::cout << "account 저장\n";
	// Info data login 후
	if (save_account_info(pc_data["account_info"], conn, id, up_time) != 0) {
        std::cout << "account info 업데이트 중 에러발생" << std::endl;
        exit(1);
    }

    std::cout << "cpu 저장\n";
	// CPU data save
	if (save_cpu_info(pc_data["cpu"], conn, id, up_time) != 0) {
        std::cout << "cpu 저장 중 에러발생" << std::endl;
        exit(1);
    }

    std::cout << "mempry 저장\n";
	// Memory data save
    if (save_memory_info(pc_data["mem"], conn, id, up_time) != 0) {
        std::cout << "memory 저장 중 에러발생" << std::endl;
        exit(1);
    }
    
    std::cout << "disk 저장\n";
	// Disk data save
	if (save_disk_info(pc_data["disk"], conn, id, up_time) != 0) {
        std::cout << "disk 저장 중 에러발생" << std::endl;
        exit(1);
    }
    
    std::cout << "nic 저장\n";
  	// NIC data save
	// network (외부와 통신하는 network)
    int nic;
    nic = save_nic_info(pc_data["nic"], conn, id, up_time);
	if (nic != 0) {
        if (nic==1)std::cout << "network 저장 중 에러발생" << std::endl;
        else if(nic==2)std::cout << "lo 저장 중 에러발생" << std::endl;
        exit(1);
    }
    returnNodeToPool(mysqlPool, conn);
}


// json 정보를 쿼리문으로 바꾸는 코드 구현 부
//  cpu 정보 저장 구현 부
int save_cpu_info(json j, MYSQL * conn, std::string id, std::string up_time){
        std::string query, q_key, q_value;
		for (int i = 0; i < j.size(); ++i) { // 코어(프로세서)의 개수만큼 반복
		q_key = "(id, up_time , Processor,  ";
		query = "INSERT INTO cpu ";
		q_value = q_value = ") VALUES ('" + id + "', '" + up_time  +  "', " + std::to_string(i) + ", ";
		for (auto& [key, value] : j[i].items()) { // 각 코어의 정보를 정제해 INSERT 쿼리문 작성
			q_key += key + ", ";
		 	if (key == "power_management" || key == "address_sizes" || key == "cache_size" || key =="microcode"||  key == "bugs" || key == "flags" || key =="fpu" || key == "fpu_exception" || key == "model_name" || key == "vendor_id" || key == "wp") {
				q_value += "'" + value.get<std::string>() + "', ";
			}else{
				q_value += value.get<std::string>() + ", ";
			}
		}
		q_key = q_key.substr(0, q_key.size()-2);
		q_value = q_value.substr(0, q_value.size()-2);
		q_value += ")";
	
		query += q_key + q_value;

		if (mysql_query(conn, query.c_str())) { // 작성된 쿼리문을 mysql server에서 실행
			std::cerr <<  mysql_error(conn) << std::endl;
			exit(1);
		}
	}
		return 0;
}


// 메모리 정보 저장 구현 부
int save_memory_info(json j, MYSQL * conn, std::string id, std::string up_time){
	std::string query, q_key, q_value;
    query = "INSERT INTO memory ";
	q_key = "(id, up_time, ";
	q_value = ") VALUES ('" + id  + "', '" + up_time  +  "', ";

	for (auto& [key, value] : j.items()) {
		q_key += key  + ", ";
		q_value += value.get<std::string>() + ", ";
	}

	q_key = q_key.substr(0, q_key.size()-2);
	q_value = q_value.substr(0, q_value.size()-2);
	q_value += ")";

	query += q_key + q_value;

	if (mysql_query(conn, query.c_str())) { // Memory 정보로 작성된 INSERT 쿼리문 실행
		std::cerr <<  mysql_error(conn) << std::endl;
        return 1;
	}
		return 0;
}


// disk 정보 저장 구현 부
int save_disk_info(json j, MYSQL * conn, std::string id, std::string up_time) {
    std::string query, q_key, q_value;
    double str_val;
	q_key = "(id, up_time, ";
	query = "INSERT INTO disk ";
    q_value = ") VALUES (" + id  + ", '" + up_time  +  "', ";

	for (auto& [key, value] : j.items()) {
		q_key += key + ", ";
		if (key == "disk_stats") q_value += "'" + value.dump()  + "', ";
		else {
			str_val = value.get<double>();
			q_value += std::to_string(str_val) + ", ";
		}
	}

	q_key = q_key.substr(0, q_key.size()-2);
	q_value = q_value.substr(0, q_value.size()-2);
	q_value += ")";
	query += q_key + q_value;

	if (mysql_query(conn, query.c_str())) { // disk 정보로 작성된 INSERT 쿼리문 실행
                std::cerr <<  mysql_error(conn) << std::endl;
                return 1;
    }

	return 0;
}


// nic 정보 저장 (로컬 루프백, 일반 network 분리)
int save_nic_info(json nic, MYSQL * conn, std::string id, std::string up_time) {
    std::string query, q_key, q_value;
	for (auto& [n_key, j] : nic.items()) { // 네트워크의 종류만큼 반복
		if(n_key != "lo"){ // local loopback 네트워크가 아닌 경우
			q_key = "(id, up_time, network_name, ";
			query = "INSERT INTO network ";
			q_value = ") VALUES ('" + id  + "', '" + up_time  +  "', '" + n_key + "', " ;
			int cnt = 0;
			for (auto& [key, value] : j.items()) {
				q_key += key + ", ";
	
				if (key == "broadcast" || key == "ether" || key =="flags"||  key == "inet" || key == "inet6" || key =="mtu" || key == "scopeid" || key =="netmask" ){ // 문자열 형식
					q_value += "'" + value.get<std::string>() + "', ";
				}else if(key == "RX" || key == "TX"){ // json 형식
					q_value += "'" + value.dump() + "', ";
				}else{ // 숫자형식
					q_value += value.get<std::string>() + ", ";
				}
			}
			q_key = q_key.substr(0, q_key.size()-2);
			q_value = q_value.substr(0, q_value.size()-2);
			q_value += ")";

			query += q_key + q_value;
			if (mysql_query(conn, query.c_str())) { 
				std::cerr <<  mysql_error(conn) << std::endl;
				return 1;
			}
		}

	 	// lo
	 	else { // local loopback 네트워크 인 경우
        		q_key = "(id, up_time, ";
        		query = "INSERT INTO lo ";
        		q_value = ") VALUES ('" + id  + "', '" + up_time  +  "', ";
        		for (auto& [key, value] : j.items()) {
               	 	q_key += key + ", ";
                
                		if (key == "mtu" || key == "prefixlen" || key =="txqeuelen" ){ // 숫자형식
                        		q_value += value.get<std::string>() + ", ";
                		}else if(key == "RX" || key == "TX"){
                        		q_value += "'" + value.dump() + "', "; // json 형식
                		}else{ // 문자 형식
                        		q_value += "'" +  value.get<std::string>() + "', ";
                		}
        		}
        		q_key = q_key.substr(0, q_key.size()-2);
        		q_value = q_value.substr(0, q_value.size()-2);
        		q_value += ")";

        		query += q_key + q_value;
        		if (mysql_query(conn, query.c_str())) {
				std::cerr <<  mysql_error(conn) << std::endl;
				return 2;
	  		}
	  	}
	}
      
	return 0;
}


// 계정정보 갱신 구현 부
int save_account_info(json j, MYSQL * conn, std::string id, std::string up_time){
	std::string query = "UPDATE account_info SET ";
    query += "up_time='" +  up_time + "', state='" + j["state"].get<std::string>() + "'" ;
    query += "WHERE id='" + id + "';";
	if (mysql_query(conn, query.c_str())) {
		    std::cerr <<  mysql_error(conn) << std::endl;
			std::cout << query << std::endl;
            return 1;
	}
	return 0;
}

