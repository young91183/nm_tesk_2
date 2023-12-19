#include "Save_Server.h"


void saveDB(json pc_data, MysqlPool *mysqlPool);

// Client PC 상태 정보 저장을 위한 Session

PC_Info_Session::PC_Info_Session(int socket, MysqlPool* pool) : client_socket(socket), mysqlPool(pool) {}


void PC_Info_Session::start() { 
    read_();
}


void PC_Info_Session::read_() {
    std::string total_buffer;
    char buffer[1024];
    ssize_t n;
    while(n = read(client_socket, buffer, sizeof(buffer)-1) > 0) { 
        total_buffer += buffer; 
        memset(buffer, 0 , sizeof(buffer)); // 버퍼 초기화
    }

    if (n < 0){
        std::perror("read");
        exit(1);
    }

    nlohmann::json j = nlohmann::json::parse(total_buffer); // dump된 메세지를 json 객체로 복구
    close(client_socket); // 소켓 종료
    saveDB(j, mysqlPool); // DB저장
}


// 로그인 상태인지 확인해서 현 상태를 반환하는 함수
int check_login_state(std::string id, MYSQL * conn) { 

    MYSQL_RES *res;
    MYSQL_ROW row;
    std::string query, state;
	int result = 1; // login, logout도 아닌 경우 1반환

    query= "SELECT state FROM account_info WHERE id='"+ id + "'";
    if (mysql_query(conn, query.c_str())) {
        std::cerr << "쿼리 입력시 문제가 발생했습니다.\n" << mysql_error(conn);
    }
    res = mysql_use_result(conn); // 쿼리문 결과 받아오기
    while ((row = mysql_fetch_row(res)) != NULL) {
        state = row[0]; // 쿼리문 결과 넘겨 받기
        if (state == "login") {
            result = 0; // 로그인 중인 경우 0 반환
            break;
        } else if (state == "logout"){
            result = 2; // 로그 아웃 중인 경우 2 반환
            break;
        }
    }

    mysql_free_result(res);

    return result;
}


/*-------------------------------- DB저장 구현 부 -------------------------------------------*/

// json 받아서 DB에 저장하는 함수
void saveDB(json pc_data, MysqlPool *mysqlPool) {
    MYSQL *conn = getNodeFromFront(mysqlPool); // Pool에 있는 Connection 불러오기
	std::string id, up_time;
	int login_check;
	id = pc_data["account_info"]["id"].get<std::string>();
	up_time = pc_data["account_info"]["up_time"].get<std::string>();

	login_check = check_login_state(id, conn);
	if(login_check == 2){ // 만약 로그인 안된 ID라면
        std::cout << id << "는 로그인 되지 않은 ID 입니다.\n";
        return;
    } else if(login_check == 1){ // ID에 대한 상태 조회 결과가 없다면
        std::cout << id << "는 존재하지 않거나 잘못된 ID 입니다.\n";
        return;
    }

	// Info data update
	if (save_account_info(pc_data["account_info"], conn, id, up_time) != 0) {
        std::cout << "account info 업데이트 중 에러발생" << std::endl;
        exit(1);
    }

	// CPU data save
	if (save_cpu_info(pc_data["cpu"], conn, id, up_time) != 0) {
        std::cout << "cpu 저장 중 에러발생" << std::endl;
        exit(1);
    }

	// Memory data save
    if (save_memory_info(pc_data["mem"], conn, id, up_time) != 0) {
        std::cout << "memory 저장 중 에러발생" << std::endl;
        exit(1);
    }
    
	// Disk data save
	if (save_disk_info(pc_data["disk"], conn, id, up_time) != 0) {
        std::cout << "disk 저장 중 에러발생" << std::endl;
        exit(1);
    }
    
  	// NIC data save
	// network (외부와 통신하는 network)
    int nic;
    nic = save_nic_info(pc_data["nic"], conn, id, up_time);
	if (nic != 0) {
        if (nic==1)std::cout << "network 저장 중 에러발생" << std::endl;
        else std::cout << "lo 저장 중 에러발생" << std::endl;
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
			for (auto& [key, value] : j[i].items()) { // 각각 코어별로 정보를 추출해 INSERT 쿼리문 작성
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
				std::cerr <<  mysql_error(conn) << std::endl << query << std::endl;
				return 1;
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

	// json 데이터로 쿼리문 작성
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
    q_value = ") VALUES ('" + id  + "', '" + up_time  +  "', ";

	// json 파일로 쿼리문 작성
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
                return 1; // 오류 발생 시 1반환
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
				return 1; // network 테이블 저장 시 오류 발생하면 1반환
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
				return 2; // lo 테이블 저장 시 오류 발생하면 2 반환
			}
	  	}
	}
      
	return 0;
}


// 계정정보 갱신
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


