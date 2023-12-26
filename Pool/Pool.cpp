#include "Pool.h"

//mysql Connection Pool 초기 생성을 위한 함수
MysqlPool createMysqlPool() {
		MysqlPool mysqlPool;
    	mysqlPool.firstNode = NULL;
    	mysqlPool.lastNode = NULL;
    	return mysqlPool;
}


//Mysql Pool에 새로운 connection을 삽입하기 위한 함수
void insertNodeAtBack(MysqlPool *mysqlPool, MYSQL* conn) {
	ConnNode *newNode = (ConnNode *)malloc(sizeof(ConnNode));
	newNode->conn = conn;
	newNode->head = mysqlPool->lastNode;
	newNode->tail = NULL;
	if (mysqlPool->firstNode != NULL && mysqlPool->lastNode == NULL) { // 만약 첫번째 노드가 비어있는 경우
		newNode -> head = mysqlPool->firstNode; 
		mysqlPool->lastNode = newNode;
		mysqlPool->firstNode->tail = newNode; 
	} else if(mysqlPool->firstNode == NULL){ 
		mysqlPool->firstNode = newNode; 
	}else{ // 정상적으로 존재할 경우
		mysqlPool->lastNode->tail = newNode;
	}
	mysqlPool->lastNode = newNode;
}


//Mysql Pool에 존재하는 Connection을 불러오기 위한 함수
MYSQL* getNodeFromFront(MysqlPool *mysqlPool) {
	std::mutex mtx; 
    std::lock_guard<std::mutex> lock(mtx);
    	if (mysqlPool->firstNode == NULL) { // 만약 Connection Pool이 비어있는 경우
    		MYSQL* conn = mysql_init(NULL);
    		std::cout << "add mysql connect!" << std::endl;
    		if (!mysql_real_connect(conn, "localhost", "root", "0000", "client_management", 0, NULL, 0)) {
                std::cerr << mysql_error(conn)<< std::endl;
                exit(1);
			}
        	insertNodeAtBack(mysqlPool, conn); // 새로운 Connection을 Pool에 추가
    	}
    	ConnNode* tempNode = mysqlPool->firstNode; // firstNode를 가져오기
    	mysqlPool->firstNode = tempNode->tail; // firstNode를 firstNode의 꼬리 Node로 변경
    	
    	if (mysqlPool->firstNode != NULL)  // firstNode의 꼬리 Node가 NULL이 아닌 경우 
    		mysqlPool->firstNode->head = NULL;  // firstNode가 된 Node의 머리를 NULL로 변경
    	else mysqlPool->lastNode = NULL;
	return tempNode->conn;
}


// 사용 완료 Connection을 Mysql Pool에 반환하기 위한 함수 
void returnNodeToPool(MysqlPool *mysqlPool, MYSQL* conn) {
	std::mutex mtx; 
	std::lock_guard<std::mutex> lock(mtx);
	ConnNode* tempNode = new ConnNode();
	tempNode->conn = conn;
	tempNode->head = mysqlPool->lastNode;
	tempNode->tail = NULL;

	if (mysqlPool->lastNode != NULL){ // 만약 마지막 노드가 살아 있는 경우
		mysqlPool->lastNode->tail = tempNode; 
		mysqlPool->lastNode = tempNode; 
	}
	else if(mysqlPool->firstNode != NULL){ // 만약 lastNode가 Null이고 firstNode는 존재하는 경우
		mysqlPool->firstNode->tail = tempNode; 
		mysqlPool->lastNode = tempNode; 
	}
	else mysqlPool->firstNode = tempNode; // Pool이 비어있을 경우 
}


// Thread Pool 생성자
// 입력된 size만큼의 스레드를 생성하고, 각 스레드가 workerThread 함수를 실행
ThreadPool::ThreadPool(int size) : size(size), stop(false) {
    threads = new std::thread[size];
    for(int i = 0; i < size; i++) {
        threads[i] = std::thread(&ThreadPool::workerThread, this);
    }
}


void ThreadPool::workerThread() {
    // 무한루프를 돌며 작업을 수행합니다.
    while(!stop) {
        std::function<void()> task;
        {   // 뮤텍스 범위 설정
            // 작업 큐에 대한 동기화를 위해 뮤텍스 잠금
            std::unique_lock<std::mutex> lock(mtx_pool);
            // 작업 큐가 비어 있거나 스레드 풀이 중단되기 전까지 대기
            // 다른 Thread가 cv 객체의 notify_one 또는 notify_all 메서드를 호출하여 대기 중인 스레드들에게 신호를 보낼 때까지 현재 스레드는 대기 상태를 유지
            cv.wait(lock, std::bind(&ThreadPool::checkStopOrNotEmptyTasks, this));
            // 스레드 풀이 중단되고 작업 큐가 비어있으면 스레드를 종료 (Thread Pool 전체 종료 시 사용되는 조건)
            if(stop && tasks.isEmpty()) return; // 처음 생성될때는 stop이 flase이므로 조건 성립 X
            // 작업 큐에서 작업 가져오기
            task = tasks.pop();
            // 작업 중인 스레드의 수를 증가
            busy_threads++;
            std::cout << "작업 시작 " << busy_threads << " <- 작업 Thread 개수\n";
        }

        // 작업을 실행 (보통 여기서 대기)
        if(task) task();

        {   // 뮤텍스 범위
            std::unique_lock<std::mutex> lock(mtx_pool);
            // 작업이 완료되면 작업 중인 스레드의 수를 감소
            busy_threads--;
            std::cout << "작업 종료 " << busy_threads << " <- 작업 Thread 개수\n";
        }
        // 서버의 모든 작업이 완료되었을 떄는 cv_end로 알림
        if (tasks.isEmpty() || busy_threads == 0) {
            cv.notify_all();
        }
    }
}


// 스레드 풀 클래스의 소멸자 구현
// 스레드 풀을 중단하고 모든 스레드를 종료
ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(mtx_pool);
        // 스레드 풀을 중단
        stop = true;
    }
    std::cout << "Threads 종료 기다리는 중...\n";
    // wait 중인 모든 스레드 깨워서 stop 변화 감지시키기
    
    for(int i = 0; i < size; i++) {
        cv.notify_all();
        std::cout << i << "번째 Thread 종료 기다리는 중...\n";
        // 각 스레드가 종료될 때까지 대기
        if(threads[i].joinable()) {
            cv.notify_all();
            threads[i].join();
        }
        std::cout << i << "번째 Thread 종료완료\n";
        //std::cout << busy_threads << " <- 남은 동작 Thread 개수\n";
    }
    delete[] threads;
}


// 스레드 풀이 중단되었는지 또는 작업 큐가 비어있지 않은지 확인
bool ThreadPool::checkStopOrNotEmptyTasks() {
    return stop || !tasks.isEmpty();
}


// 작업을 작업 큐에 추가합니다.
void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(mtx_pool);
        // 작업을 큐에 추가
        tasks.push(task);
    }
    // 작업이 추가됨을 알림
    cv.notify_one();
}


// 작업 큐가 비어있는지와 작업 중인 스레드가 없는지 확인
/*bool ThreadPool::checkEmptyTasksAndBusyThreads() {
    return tasks.isEmpty() || (busy_threads == 0);
}*/

/*
void ThreadPool::join() {
    std::unique_lock<std::mutex> lock(mtx_pool);
    // 작업 큐가 비어있고, 작업 중인 스레드가 없을 때까지 대기
    cv_end.wait(lock, std::bind(&ThreadPool::checkEmptyTasksAndBusyThreads, this));
}*/

