#include "Pool.h"

std::mutex mtx; 

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

// ThreadPool 클래스의 구현
ThreadPool::ThreadPool(int size) {
	threads = new std::thread[size];
	for(int i = 0; i < size; i++) {
		threads[i] = std::thread([this] {
			while(true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(mtx_pool);
                	cv.wait(lock, std::bind(&ThreadPool::checkStopOrNotEmptyTasks, this));
					if(stop && tasks.isEmpty()) return;
					task = tasks.pop();
					busy_threads++;
				}

				if(task) task();
				{
					std::unique_lock<std::mutex> lock(mtx_pool);
					busy_threads--;
					if (tasks.isEmpty() && busy_threads == 0) {
							cv_end.notify_all();
					}
				}
			}
		});
	}
}


ThreadPool::~ThreadPool() {
    {
    	std::unique_lock<std::mutex> lock(mtx_pool);
    	stop = true;
	}
	cv.notify_all();
	for(int i = 0; i < size; i++) {
		if(threads[i].joinable()) threads[i].join();
    }
	delete[] threads;
}

bool ThreadPool::checkStopOrNotEmptyTasks() {
    return stop || !tasks.isEmpty();
}

void ThreadPool::enqueue(std::function<void()> task) {
    	{
   		std::unique_lock<std::mutex> lock(mtx_pool);
   		tasks.push(task);
        }
    cv.notify_one();
}

bool ThreadPool::checkEmptyTasksAndBusyThreads() {
	return tasks.isEmpty() && (busy_threads == 0);
}

void ThreadPool::join() {
    std::unique_lock<std::mutex> lock(mtx_pool);
    cv_end.wait(lock, std::bind(&ThreadPool::checkEmptyTasksAndBusyThreads, this));
}
