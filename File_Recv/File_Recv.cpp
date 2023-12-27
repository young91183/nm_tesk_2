#include "File_Recv.h"
// File_Recv Session 구현
File_Recv::File_Recv(MysqlPool *pool, int recv_socket, std::string id) : mysqlPool(pool), client_socket(recv_socket), client_id(id){}

void File_Recv::start() {
    ssize_t n;
    std::string file_name, check_buffer;
    file_name = "./recv_files/" + client_id + "_comp_file";
    std::ofstream received_file(file_name.c_str(), std::ios_base::binary);
    char size_buf[100];

    // 전송 될 파일 크기 받기
    n = read(client_socket, size_buf, sizeof(size_buf)); 
    if (n < 0){
        std::perror("read");
        std::cout << "Client request error\n";
        exit(1);
    }

    int size = std::stoi(size_buf);
    //std::cout << size << " Byte \n";

    char* file_buffer = new char[4048];
    ssize_t bytes_received;
    int total_size = 0;

    // 파일 전송 받기
    while (true) {
        bytes_received = read(client_socket, file_buffer, sizeof(file_buffer));
        check_buffer = file_buffer;
        total_size += bytes_received;
        if(total_size >= size) break;
        received_file.write(file_buffer, sizeof(file_buffer));
        memset(file_buffer, 0, sizeof(file_buffer)); // 버퍼 초기화
    }

    delete[] file_buffer;
    received_file.close(); // 파일을 닫는다.
    std::cout << "\n▶ " << client_id << "로부터 파일 전송받기 완료!\n";
    if(!(write_log_db(mysqlPool, "File_Recv : file_recv_success", client_id))) exit(1);
    delete this;
}

/*----------------파일 정보 수신 실패작-----------------------------------------*/

// 파일 정보 한번에 받기 실패
/*
if (read(client_socket, file_buffer, size) != size) {
    printf("파일 데이터 수신 오류\n");
    free(file_buffer);
    close(client_socket);
    received_file.write(file_buffer, size);
    exit(1);
}
received_file.write(file_buffer, size);
*/

// 파일 정보 순차적으로 받고, EOS 또는 압축 에러 캐치해서 중단하기
/*
while (true) {
    bytes_received = read(client_socket, file_buffer, sizeof(file_buffer));
    check_buffer = file_buffer;
    std::cout << check_buffer << sed::endl;
    if(check_buffer == "<!EOS!>") break;
    else if(check_buffer == "<!lz4_ERR!>"){
        std::cout << "lz4 압축 중 오류 발생\n";
        if(!(write_log_db(mysqlPool, "File_Recv : lz4_Err", client_id))) exit(1);
        delete this;
        return;
    }
    // EOS 신호를 검사 후 
    received_file.write(file_buffer, sizeof(file_buffer));
    memset(file_buffer, 0, sizeof(file_buffer)); // 버퍼 초기화
}
*/


/*
 while ((bytes_received = read(client_socket, file_buffer, sizeof(file_buffer))) > 0) {
        received_data.append(file_buffer, bytes_received);
        size_t found_pos = received_data.find(end_sign);
        if (found_pos != std::string::npos) {
            received_data.erase(found_pos, end_sign.length());
            break;
        } else if( found_pos == "<!lz4_EOS!>")
        received_file.write(received_data.c_str(), received_data.size());
        received_data.clear();
        memset(file_buffer, 0, sizeof(file_buffer));
    }
*/
