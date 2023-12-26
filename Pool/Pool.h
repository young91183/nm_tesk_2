#ifndef POOL_H
#define POOL_H

#include <mysql/mysql.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <iostream>

// mysql 연결 풀을 위한 Node
typedef struct ConnNode {
    struct ConnNode *head;
    struct ConnNode *tail;
    MYSQL* conn;
} ConnNode;

// mysql 연결 풀의 Hdr
typedef struct MysqlPool {
    ConnNode *firstNode;
    ConnNode *lastNode;
} MysqlPool;

MysqlPool createMysqlPool();
void insertNodeAtBack(MysqlPool *mysqlPool, MYSQL* conn);
MYSQL* getNodeFromFront(MysqlPool *mysqlPool);
void returnNodeToPool(MysqlPool *mysqlPool, MYSQL* conn);


// ThreadPool 클래스 정의 시작
class ThreadPool {
private:
    // Node 클래스 정의 시작
	class Node {
	public:
		// 작업을 저장할 함수 객체
		std::function<void()> task;
		// 다음 노드를 가리키는 포인터
		Node* next;

        // 생성자. 작업을 받아와 task에 저장하고, next는 nullptr로 초기화
		Node(std::function<void()> task) : task(task), next(nullptr) {}
	};

    // Queue 클래스 정의 시작
	class Queue {
	private:
		// 큐의 첫 번째 노드를 가리키는 포인터
		Node* front;
		// 큐의 마지막 노드를 가리키는 포인터
		Node* last;

	public:
		// 생성자. front와 last를 nullptr로 초기화
		Queue() : front(nullptr), last(nullptr) {}

        // 새 작업을 큐에 추가하는 메소드
		void push(std::function<void()> task) {
			// 새 노드 생성
			Node* node = new Node(task);
			
			// 큐가 비어 있으면 front와 last 모두 새 노드를 가리키게 함
			if (isEmpty()) {
				front = last = node;
			} 
			// 큐가 비어 있지 않으면 last가 가리키는 노드의 next를 새 노드로 설정하고 last를 새 노드로 업데이트
			else {
				last->next = node;
				last = node;
			}
		}

        // 큐에서 작업을 꺼내는 메소드
		std::function<void()> pop() {
			// 큐가 비어 있으면 nullptr 반환
			if (isEmpty()) {
				return nullptr;
			}
			
			// 큐가 비어 있지 않으면 front 노드의 작업을 저장하고 front를 다음 노드로 업데이트
			Node* temp = front;
			std::function<void()> task = temp->task;
			front = front->next;
			
			// front가 nullptr이면 last도 nullptr로 설정
			if (front == nullptr) {
				last = nullptr;
			}
			
			// 원래 front 노드 삭제
			delete temp;
			
			// 작업 반환
			return task;
		}

		// 큐가 비어 있는지 확인하는 메소드
		bool isEmpty() {
			return front == nullptr;
		}
	};

	std::thread* threads; // 스레드 풀로 사용될 스레드 배열
	int size, busy_threads = 0; // 스레드 풀의 크기와 바쁜 스레드 수
	Queue tasks; // 작업을 저장할 큐
	bool stop;  // 스레드 풀 종료 여부
	std::mutex mtx_pool; // 스레드 간 동기화를 위한 뮤텍스
	std::condition_variable cv, cv_end; // 컨디션 변수

public:
    // 스레드 풀 생성자와 소멸자
    ThreadPool(int size);
    ~ThreadPool();
	
    // 작업 스레드 함수
	void workerThread();
	
    // 작업을 큐에 추가하는 메소드
    void enqueue(std::function<void()> task);

    // 모든 작업이 완료될 때까지 기다리는 메소드
    //void join();

    // 종료 여부와 작업 큐의 상태를 확인하는 메소드
	bool checkStopOrNotEmptyTasks();
	//bool checkEmptyTasksAndBusyThreads() ;
};
#endif // POOL_H
