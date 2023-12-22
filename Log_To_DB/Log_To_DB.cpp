#include "Log_To_DB.h"

std::string getCurrentDateTime() {
	auto now = std::chrono::system_clock::now();
	std::time_t now_t = std::chrono::system_clock::to_time_t(now);
	std::tm* now_tm = std::localtime(&now_t);

	char buffer[100];
	std::strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", now_tm); // date time 형태로 가공

	return std::string(buffer);
}

bool write_log_db(MysqlPool *mysqlPool, std::string action, std::string id){
    std::string query = "", q_value = "", up_time;
    query += "INSERT INTO server_log (id, up_time,  action) VALUE (";
    up_time = getCurrentDateTime();
    q_value += "'" + id + "', '" + up_time + "', '" + action + "')";
    query += q_value;
    MYSQL *conn = getNodeFromFront(mysqlPool);
    if (mysql_query(conn, query.c_str())) { // Memory 정보로 작성된 INSERT 쿼리문 실행
		std::cerr <<  mysql_error(conn) << std::endl;
        return false;
	}
    returnNodeToPool(mysqlPool, conn);
    return true;
}
