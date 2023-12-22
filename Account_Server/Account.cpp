#include "Account.h"

/*------------------------ 인코딩 관련 함수 ------------------------*/
// 계정인증에서 사용되는 인코딩 관련 함수 정의 부

// Base64 인코딩 함수
std::string base64(const unsigned char* buffer, size_t length) {
	// Base64를 처리하는 BIO를 생성
	BIO* b64 = BIO_new(BIO_f_base64());
	// 개행 없이 Base64를 처리하도록 설정
	BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
	// 메모리 내부의 BIO를 생성하고 b64와 연결
	BIO* bio = BIO_new(BIO_s_mem());
	bio = BIO_push(b64, bio); 

	// 데이터를 쓰고 flush
	BIO_write(bio, buffer, length);
	BIO_flush(bio);

	// BIO에 저장된 데이터 호출
	char* data;
	long len = BIO_get_mem_data(bio, &data);

	// 데이터를 std::string으로 변환
	std::string encodedData(data, len);
	// BIO를 해제
	BIO_free_all(bio);

	return encodedData;
}

// SHA-512 해시 함수
std::string sha512(const std::string& data) {
	unsigned char hash[SHA512_DIGEST_LENGTH]; // 해시 값을 저장할 배열
	SHA512_CTX sha512; // SHA-512 컨텍스트
	SHA512_Init(&sha512); // 컨텍스트 초기화
	SHA512_Update(&sha512, data.c_str(), data.length()); // 데이터 업데이트
	SHA512_Final(hash, &sha512); // 해싱 종료

	// 해시 값을 std::string으로 변환하여 반환
	return std::string(reinterpret_cast<char*>(hash), SHA512_DIGEST_LENGTH);
}


/*------------------------ 계정 정보처리 Class ------------------------*/
Account_Session::Account_Session(int socket) : client_socket(socket) {}


nlohmann::json Account_Session::start() {
	read_();

	return login_id; // acoount 모듈에서 어떤 동작을 했는지 반환
}

// 계정인증 정보 수신 + 결과 반환
void Account_Session::read_() {
	std::string total_buffer, result = "", pw_temp;
	char buffer[500];
	ssize_t n;
	nlohmann::json j;

	// MYSQL Connection 생성
	MYSQL *conn;
	const char* server = "localhost";
	const char* user = "root";
	const char* password = "0000";
	const char* database = "client_management";
	
	conn = mysql_init(NULL);
	if(!mysql_real_connect(conn, server, user, password, database, 0, NULL, 0)) {
		std::cerr << "mysql database err" << mysql_error(conn)<< std::endl;
		exit(1); // MYSQL 서버에 이상이 있는 경우 서버 종료
	}
	
	while((n = read(client_socket, buffer, sizeof(buffer)-1)) > 0) {
		total_buffer += buffer;
		if(buffer[n-1] == '\0') break;
		memset(buffer, 0 , sizeof(buffer)); // 버퍼 초기화
	}

	if (n < 0){ 
		std::perror("read");
		result = "Server read error";
	} else {
		j = nlohmann::json::parse(total_buffer); // dump된 메세지를 json 객체로 복구
		pw_temp = j["password"].get<std::string>();
		pw_temp += j["id"].get<std::string>();
		pw_temp = sha512(pw_temp);
		j["password"] = base64(reinterpret_cast<const unsigned char*>(pw_temp.c_str()), pw_temp.size());
		if (j["request"] == "login") result += login(j, conn); // 요청한 account 동작이 로그인인 경우
		else if (j["request"] == "logout") result += logout(j, conn); // 요청한 account 동작이 로그아웃인 경우
		else if (j["request"] == "join") result += join(j, conn); // 요청한 account 동작이 회원가입인 경우
		else result = "잘못된 요청입니다."; // 요청한 account 동작이 목록에 없는 경우
	}

	login_id["res"] = result;
	login_id["id"] = j["id"];

	result += '\0';

	//write result 진행
	n = write(client_socket, result.c_str(), result.size());
	if (n <= 0) {
		if (n < 0) std::cout << "write err\n";
	}

	// 여기서 로그 기록 필요!
	//close(client_socket); // 소켓 종료
	mysql_close(conn);
}


// 로그인 구현
std::string Account_Session::login(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result, query, id, pw, up_time, ip, state;
	id = account["id"].get<std::string>(); // 아이디 정보 추출
	pw = account["password"].get<std::string>(); // 비밀번호 정보 추출
	up_time = account["up_time"].get<std::string>(); // 시간 정보 추출
	ip = account["ip"].get<std::string>(); // ip 정보 추출

	// 아이디 비밀번호 조회
	query = "SELECT state FROM account_info WHERE id='" + id + "' AND password='" + pw + "';";
	if(mysql_query(conn, query.c_str())) {
		std::cerr << mysql_error(conn) <<std::endl;
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "아이디, 비밀번호 조회에 실패했습니다.";
		return result;
	}
	res = mysql_use_result(conn); // SELECT 결과 저장

	// SELECT 결과 분석
	if ((row = mysql_fetch_row(res)) != NULL) {
		state = std::string(row[0]);
	} else { // 비밀번호 아이디가 일치하지 않아 조화 결과가 없을 때
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "잘못된 아이디와 비밀번호 입니다.";
		return result;
	}

	// 로그인 진행
	mysql_free_result(res);
	if (state == "login") { // 해당 아이디가 이미 login 중인 경우
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "이미 로그인 된 계정입니다.";
		return result;
	} else if (state == "logout" && state !="") { // 해당 아이디가 로그인 가능할 때
		query = "UPDATE account_info SET state='login', ip='" + ip +  "', up_time='" + up_time + "' WHERE id='" + id + "';";
		if(mysql_query(conn, query.c_str())) {
			std::cerr << mysql_error(conn) <<std::endl;
			res = mysql_use_result(conn);
			mysql_free_result(res);
			result = "로그인 정보 업데이트 오류 발생 : " + query;
			return result;
		}
	}

	// 이상없이 로그인 정보 업데이트 성공하면
	res = mysql_use_result(conn);
	mysql_free_result(res);
	result = "l_s"; // 정상적으로 로그인 되었다면 l_s 신호 반환
	return result;
}


// 로그아웃 구현 부
std::string Account_Session::logout(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result, query, id, pw, up_time, ip, state;
	id = account["id"].get<std::string>(); // 아이디 정보 추출
	pw = account["password"].get<std::string>(); // 비밀번호 정보 추출
	up_time = account["up_time"].get<std::string>(); // 시간 정보 추출
	ip = account["ip"].get<std::string>(); // ip 정보 추출

	// 아이디 비밀번호 확인
	query = "SELECT state FROM account_info WHERE id='" + id + "' AND password='" + pw + "';";
	if(mysql_query(conn, query.c_str())) {
		std::cerr << mysql_error(conn) <<std::endl;
		res = mysql_use_result(conn);
		mysql_free_result(res);
		return mysql_error(conn);
	}

	// SELECT 결과 분석
	res = mysql_use_result(conn);
	if ((row = mysql_fetch_row(res)) != NULL) { // 조회 결과가 존재 할 때
		state = std::string(row[0]);
	} else { // 비밀번호 아이디가 일치하지 않을 때
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "잘못된 아이디와 비밀번호 입니다.";
		return result;
	}
	// 아이디 비밀번호 확인 완료

	mysql_free_result(res);
	// 로그아웃 DB 업데이트
	if (state == "logout") { // 해당 아이디가 이미 logout 상태인 경우
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "이미 로그아웃 중인 계정입니다.";
		return result;
	} else if (state == "login" && state !="") { // 해당 아이디가 로그인 중일 때
		query = "UPDATE account_info SET state='logout', ip='" + ip +  "', up_time='" + up_time + "' WHERE id='" + id + "';";
		if(mysql_query(conn, query.c_str())) {
			std::cerr << mysql_error(conn) <<std::endl;
			res = mysql_use_result(conn);
			mysql_free_result(res);
			result = "로그아웃 업데이트에 실패했습니다 : " + query;
			return result;
		}
	}
	res = mysql_use_result(conn);
	mysql_free_result(res);
	result = "lo_s"; // 정상적으로 로그아웃 되었다면 lo_s 신호 반환
	return result;
}


// 회원가입 구현 부
std::string Account_Session::join(json account, MYSQL *conn) {
	MYSQL_RES *res;
	MYSQL_ROW row;
	std::string result = "join_ok";
	std::string q_key, query, q_value;
	
	// INSERT 쿼리문 작성
	query = "INSERT INTO account_info ";
	q_key = "(state, ";
	q_value = ") VALUES ('logout', ";
	
	for (auto& [key, value] : account.items()) { // json 파일 각 요소와 값 추출해 쿼리문 작성
		if(key == "request") continue;
		q_key += key + ", ";
		q_value += "'" + value.get<std::string>() + "', ";
	}
	q_key = q_key.substr(0, q_key.size()-2);
	q_value = q_value.substr(0, q_value.size()-2);
	q_value += ")";
	query += q_key + q_value;

	// INSERT 진행
	if(mysql_query(conn, query.c_str())) { // 유니크한 ID에 중복이 발생해 오류가 발생하는지 확인
		res = mysql_use_result(conn);
		mysql_free_result(res);
		result = "회원 가입에 실패했습니다. (이미 있는 계정이거나 잘못된 입력입니다.)";
		return result;
	}

	// 문제없이 INSERT가 되었다면
	res = mysql_use_result(conn);
	mysql_free_result(res);
	result = "j_s"; // 정상적으로 회원가입 되었다면 j_s 신호 반환
	return result;
}


