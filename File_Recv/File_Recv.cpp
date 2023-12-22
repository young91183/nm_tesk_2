#include "File_Recv.h"
// File_Recv Session 구현
File_Recv::File_Recv(MysqlPool *pool, int recv_socket, std::string id) : mysqlPool(pool), client_socket(recv_socket), client_id(id){}

void File_Recv::start() {
    std::cout << client_id << "로부터 파일 전송받기 시작!\n";
    std::string file_name, check_buffer;
    char file_buffer[512];
    ssize_t bytes_received;

    file_name = "./recv_files/" + client_id + "_comp_file.lz4";
    std::ofstream received_file(file_name.c_str(), std::ios_base::binary);

    while ((bytes_received = read(client_socket, file_buffer, sizeof(file_buffer))) > 0) {
        check_buffer = file_buffer;
        if(check_buffer == "<!EOS!>") break;
        // EOS 신호를 검사 후 
        received_file.write(file_buffer, bytes_received);
        // 버퍼 초기화
        memset(file_buffer, 0, sizeof(file_buffer));
    }
    received_file.close(); // 파일을 닫는다.
    std::cout << client_id << "로부터 파일 전송받기 완료!\n";
    if(!(write_log_db(mysqlPool, "File_Recv : file_recv_success", client_id))) exit(1);
    delete this;
}
