#pragma once
#include <thread>
#include <functional>
#include <iostream>
#include <inttypes.h>
#include <functional>


class Thread
{
public:

	Thread(std::atomic_bool& runable ) noexcept
		:m_runable{ runable }{
	}
	Thread() = delete;
	Thread(const Thread & other) = delete;
	~Thread() {

	}
	void virtual Thread_Job() {

	}
	bool  Run() {
		if (runable()) {
			m_t = std::move(std::thread{ &Thread::Thread_Job,this });
			return true;
		}
		else
			return false;
	}
	bool runable() {
		return m_runable;
	}
	void Join() {
			this->m_t.join();			
	}
protected:
	std::thread m_t;
	std::atomic_bool & m_runable;
};

