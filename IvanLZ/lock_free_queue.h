#pragma once
#include<iostream>
#include<atomic>
#include<thread>
#include<assert.h>
#include <mutex>
#include <queue>
//
template<typename T>
class LockFreeLinkedQueue {
public:
	//��֤��ʼ���ڵ��߳������
	LockFreeLinkedQueue() {
	}
	~LockFreeLinkedQueue() {
	}
	bool is_lock_free() {
		return false;
	}

	bool isEmpty() { return m_queue.empty(); }
	bool isFull() { return false; }

	//push������CAS��tail��
	bool push(T val);

	//pop������CAS��head��
	bool tryPop(T& val);
	//Clear ֻ����һ���̵߳��ã�
	void Clear() {
		while (!m_queue.empty())
			m_queue.pop();
	}
	int Count() {
		return (int)m_queue.size();
	}

	int test;
private:
	std::queue<T> m_queue;
	std::mutex Mtx;
};


//push������CAS��tail��
template<typename T>
bool LockFreeLinkedQueue<T>::push(T val) {
	{
		std::lock_guard<std::mutex> lg{ Mtx };
		m_queue.push(val);
	}
	return true;
}

//pop������CAS��head��
template<typename T>
bool LockFreeLinkedQueue<T>::tryPop(T& val) {
	{
		std::lock_guard<std::mutex> lg{ Mtx };
		if (m_queue.empty())return false;
		val = m_queue.front();
		m_queue.pop();

	}
	return true;

}

