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


class ThreadPool {
private:
	class Node {
	public:
		std::function<void()> task;
		Node* next;

		Node(std::function<void()> task) : task(task), next(nullptr) {}
	};

	class Queue {
	private:
	Node* front;
	Node* last;

	public:
	Queue() : front(nullptr), last(nullptr) {}

	void push(std::function<void()> task) {
		Node* node = new Node(task);
		if (isEmpty()) {
			front = last = node;
		} else {
			last->next = node;
			last = node;
		}
	}

	std::function<void()> pop() {
		if (isEmpty()) {
			return nullptr;
		}
		Node* temp = front;
		std::function<void()> task = temp->task;
		front = front->next;
		if (front == nullptr) {
			last = nullptr;
		}
		delete temp;
		return task;
	}

		bool isEmpty() {
			return front == nullptr;
		}
	};

    	std::thread* threads;
    	int size, busy_threads = 0;
    	Queue tasks;
    	bool stop;
    	std::mutex mtx_pool;
    	std::condition_variable cv, cv_end;

 	
public:
    ThreadPool(int size);
    ~ThreadPool();
    void enqueue(std::function<void()> task);
    void join();
	bool checkStopOrNotEmptyTasks();
	bool checkEmptyTasksAndBusyThreads() ;
};

#endif // POOL_H
