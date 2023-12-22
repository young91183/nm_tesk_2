#ifndef LOG_TO_DB_H
#define LOG_TO_DB_H
#include "../Pool/Pool.h"

bool write_log_db(MysqlPool *mysqlPool, std::string action, std::string id);
std::string getCurrentDateTime();

#endif // SERVER_H
