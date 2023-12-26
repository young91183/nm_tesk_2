#include "Server.h"


// 소멸 시 Thread Pool 삭제 -> 서버 종료
Server::~Server() {
    std::cout << "활성화 소켓 close\n";
    close(server_socket);
    {
        std::unique_lock<std::mutex> lock(mtx_server);
        FD_CLR(server_socket, &master_set); // close된 client_socket 목록에서 삭제
    }
    std::cout << server_socket << " <- 서버 소켓 처리\n";

    struct timeval timeout;
    timeout.tv_sec = 1;  // 초 단위 시간
    timeout.tv_usec = 0;  // 마이크로초 단위 시간
    if ((select(max_sd + 1, &master_set, nullptr, nullptr, &timeout) < 0)) {
        //std::cout << "활성화 소켓 존재\n";
        for (int fd = 0; fd <= max_sd; fd++){
            //std::cout << fd << " <- 소켓 처리\n";
            if (!FD_ISSET(fd, &master_set)){
                std::cout << fd << " <- 비활성화 소켓\n";
                close(fd);
                continue;
            }
            std::cout << fd << " <- 활성화 소켓\n";
            {
                std::unique_lock<std::mutex> lock(mtx_server);
                FD_CLR(fd, &master_set); // close된 client_socket 목록에서 삭제
            }
            close(fd);
        }
    }
    //std::cout << "소켓 닫기 성공\n";
    std::cout << "Thread_Pool 소멸 시작\n";
	delete threadPool;
    std::cout << "Thread Pool 소멸 완료\n";
}

/*
void Server::ac_start_wrapper(int i) {
        ac_start();
}*/

Server::Server(MysqlPool* pool) : mysqlPool(pool), isLoop(true)  {
	threadPool = new ThreadPool(5); // 5개의 ThreadPool 생성
	int i, port = 8081;
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
	FD_ZERO(&master_set); // master_set 초기화 (소켓이 들어갈 집합의 초기화)
    FD_SET(server_socket, &master_set); // 서버 소켓을 master_set에 추가
	max_sd = server_socket;
	for (i = 0; i < 5; i++) {
        // 작업 큐에 작업을 추가하고 잠든 Thread를 깨워 작업을 Thread에 추가
        threadPool->enqueue(std::bind(&Server::ac_start, this)); // std::function<void()>로 함수정보와 객체정보 전달
    }
}


void Server::ac_start() { // 멀티 Thread 에서 진행
    // master_set의 복사본을 만들어 select 함수에 전달 -> 서버 소켓 전달
    // 계정 정보 동작을 반환받기 위한 변수
    nlohmann::json account_res;
    int temp_socket;
    std::string res_id;
    
    // 무한 루프를 돌면서 클라이언트 요청을 계속 확인
    while (isLoop) { // 이후 isLoop로 조건을 지정해서 압축파일 Control 하는 부분에서 조건문 통제
        char buffer[500];  // 클라이언트 요청을 저장할 버퍼
        std::string total_buffer, req_buf;
        ssize_t n;
        struct timeval timeout;
        timeout.tv_sec = 2;  // 초 단위 시간
        timeout.tv_usec = 0;  // 마이크로초 단위 시간

        fd_set copy_set = master_set;
        // select 함수를 통해 작업 가능한 소켓이 있는지 확인
        if (select(max_sd + 1, &copy_set, nullptr, nullptr, &timeout) < 0) {
            std::perror("select");
            break;
        } else if (select(max_sd + 1, &copy_set, nullptr, nullptr, &timeout) == 0)
            continue;
        // 모든 소켓을 순회하면서 작업 가능한 소켓이 있는지 확인
        for (int fd = 0; fd <= max_sd; fd++) {
            // 해당 소켓이 작업 가능한 상태가 아니라면 건너뛴다
            if (!FD_ISSET(fd, &copy_set)) continue;

            if (fd == server_socket) { 
                // 서버 소캣인 경우
                sockaddr_in client_address{};
                socklen_t client_len = sizeof(client_address);
                int client_socket = accept(fd, (struct sockaddr*)&client_address, &client_len);
                if (client_socket < 0) {
                    std::perror("accept");
                    continue;
                }

                // 새로운 클라이언트 소켓을 master_set에 추가
                {
                    std::unique_lock<std::mutex> lock(mtx_server);
                    FD_SET(client_socket, &master_set);
                }
                // max_sd 업데이트
                if (client_socket > max_sd) {
                    {
                        std::unique_lock<std::mutex> lock(mtx_server);
                        max_sd = client_socket;
                    }
                }

            } else { // client socket 인 경우
                n = read(fd, buffer, 500);
                if (n < 0){
                    std::perror("read");
                    std::cout << "Client request error\n";
                    exit(1);
                }
                {
                    std::unique_lock<std::mutex> lock(mtx_server);
                    FD_CLR(fd, &master_set); // client_socket select 목록에서 삭제
                }
                req_buf = buffer;
                
                // 클라이언트 요청이 "account"인 경우, 계정 관련 처리를 수행
                if (req_buf == "account"){
                    Account_Session* account_session = new Account_Session(fd);
                    account_res = account_session->start();
                    delete account_session;
                    res_id = account_res["id"].get<std::string>();
                    if (account_res["res"].get<std::string>() == "l_s"){
                        client_socket_json[res_id] = fd;
                        if(!(write_log_db(mysqlPool, "Accouint : login_success", res_id))) exit(1);
                    } else if (account_res["res"].get<std::string>() == "lo_s") {
                        try{
                            if(!(write_log_db(mysqlPool, "Accouint : logout_success", res_id))) exit(1);
                            close(client_socket_json[res_id]);
                            client_socket_json.erase(res_id);
                        } catch(json::type_error& e){}
                        close(fd);
                    } else {
                        if(account_res["res"].get<std::string>() == "j_s"){
                             if(!(write_log_db(mysqlPool, "Accouint : account_join_success", res_id))) exit(1);
                        } else {
                            std::string err_log = "Accouint : abnormal account request : " + res_id ;
                            if(!(write_log_db(mysqlPool, err_log, "_server"))) exit(1);
                        }
                        close(fd);
                    }
                }
                // 클라이언트 요청이 "pc_info"인 경우, PC 상태 정보 관련 처리를 수행
                else if (req_buf == "pc_info"){
                    PC_Info_Session* pc_info_session = new PC_Info_Session(fd, mysqlPool);
                    pc_info_session->start();
                    delete pc_info_session;
                }
            }
        } // for 문 종료
    } // while문 종료
}

// 서버 동작 부 -> 파일요청 및 시스템 종료 제어
void Server::request_file() {
    // 구현 중
    int request_socket;
    std::string req, id, dir_path, comp_err_msg;
    char buffer[512];
    ssize_t n;

    while(isLoop){
        std::cout << "[목록 : req_file, show_client , quit]\n";
        std::cout << "Request : ";
        std::cin >> req;
        if (req == "quit") {
            isLoop = false;
            if(!(write_log_db(mysqlPool, "Server : Server_OFF", "_server"))) exit(1);
            break;
        } else if (req == "show_client"){
            std::cout << "로그인 된 ID 목록 \n" << client_socket_json << std::endl;
            continue;
        } else if (req == "req_file"){
            std::cout << "ID : ";
            std::cin >> id;
            try {
                request_socket = client_socket_json[id.c_str()].get<int>();
            } catch (json::type_error& e) {
                std::cout << "로그인 되지 않은 ID입니다. 처음부터 다시 입력해 주십시오.\n";
                continue;
            }

            std::cout << "파일 경로 : ";
            std::cin >> dir_path;
            dir_path += '\0';

            // 파일 경로를 전송
            n = write(request_socket, dir_path.c_str(), dir_path.size());
            if (n <= 0) {
                if (n < 0) std::cout << "write err\n";
                exit(1);
            }
            if(!(write_log_db(mysqlPool, "File_Recv : req_comp_file_to_client", id))) exit(1);
            memset(buffer, 0 , sizeof(buffer)); 
            // 압축파일 생성 중 이상 없는지 전달 받기
            n = read(request_socket, buffer, sizeof(buffer)-1);
            if (n < 0){
                std::perror("read");
                std::cout << "Client request error\n";
                exit(1);
            }
            
            comp_err_msg = buffer;
            if(comp_err_msg == "path_err"){
                std::cout << "잘못된 경로입니다. 처음부터 다시 입력해 주십시오.\n";
                if(!(write_log_db(mysqlPool, "File_Recv : req_file_path_err", id))) exit(1);
                continue;
            }

            std::cout << comp_err_msg << std::endl;
            if(!(write_log_db(mysqlPool, "File_Recv : start_file_recv", id))) exit(1);
            File_Recv* file_recv = new File_Recv(mysqlPool, request_socket, id);
            std::thread(&File_Recv::start, file_recv).detach();
            std::cout << id << "로부터 파일 전송 받기 시작!\n";
        } else {
            std::cout << "잘못된 요청입니다. 다시 입력해 주세요.\n";
            continue;
        }
        // 기능 추가 필요 시 추가
    }
    std::cout << "서버 종료\n";
}

