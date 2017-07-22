// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.
// Added for Ishiiruka By Tino

// Copyright 2014 Rodolfo Bogado
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the owner nor the names of its contributors may
//       be used to endorse or promote products derived from this software
//       without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#pragma once
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

#include "Common/Thread.h"

namespace Common
{

// based on the 
// boost smart_ptr's yield_k algorithm.
// yields the cpu based on the contention amount
inline void cYield(size_t count)
{
	if (count < 16)
	{
		Common::YieldCPU();
	}
	else if (count < 32)
	{
		SleepCurrentThread(0);
	}
	else
	{
		SleepCurrentThread(1);
	}
}

// Basic Spin Lock using atomic flag
template<bool ContentionControl>
class SpinLock
{
private:
	std::atomic_flag lck;
public:
	__forceinline SpinLock()
	{
		lck.clear();
	}
	__forceinline void lock()
	{
		size_t count = 0;
		while (lck.test_and_set(std::memory_order_acquire))
		{
			if (ContentionControl)
				cYield(count++);
		}
	}

	__forceinline void unlock()
	{
		lck.clear(std::memory_order_release);
	}
};

// One Producer - One Consumer lock free concurrent queue
template <typename T>
class OneToOneQueue
{
protected:
	const size_t objectsize = sizeof(T);
	const size_t pagesize = (sizeof(T) > 32) ? 8 : (256 / sizeof(T));
	struct QueueNode
	{
		QueueNode(): next(nullptr), value()
		{

		}
		QueueNode(const T &val): next(nullptr), value(val)
		{}
		QueueNode(T &&val): next(nullptr), value(std::move(val))
		{}
		T value;
		std::atomic<QueueNode*> next;
	};
	std::vector<std::unique_ptr<QueueNode[]>> m_pages;
	QueueNode *m_first, *m_nodepool, *m_last;
	std::atomic<QueueNode*> m_limit;
	inline void Advance()
	{
		m_last = m_last->next.load();
		QueueNode *currentlimit = m_limit.load();
		QueueNode *localfirst = m_first, *current = nullptr;
		while (m_first != currentlimit)
		{
			current = m_first;
			m_first = m_first->next.load();
		}
		if (localfirst != m_first)
		{
			current->next.store(m_nodepool);
			m_nodepool = localfirst;
		}
	}
	void AddPages(size_t count)
	{
		for (size_t i = 0; i < count; i++)
		{
			QueueNode* current = new QueueNode[pagesize];
			m_pages.push_back(std::move(std::unique_ptr<QueueNode[]>(current)));
			for (size_t j = 0; j < pagesize; j++)
			{
				current[j].next.store(m_nodepool);
				m_nodepool = &current[j];
			}
		}
	}
	inline QueueNode *GetNewNode()
	{
		if (m_nodepool == nullptr)
		{
			AddPages(1);
		}
		QueueNode* current = m_nodepool;
		m_nodepool = m_nodepool->next.load();
		current->next.store(nullptr);
		return current;
	}
public:
	OneToOneQueue(): m_pages()
	{
		m_nodepool = nullptr;
		AddPages(1);
		m_first = m_last = GetNewNode();
		m_limit.store(m_last);
	}

	OneToOneQueue(size_t capacity): m_pages()
	{
		m_nodepool = nullptr;
		size_t numpages = capacity / pagesize;
		numpages = numpages < 1 ? 1 : numpages;
		AddPages(numpages);
		m_first = m_last = GetNewNode();
		m_limit.store(m_last);
	}

	virtual ~OneToOneQueue()
	{

	}

	inline bool push(const T &item)
	{
		QueueNode* current = GetNewNode();
		current->value = item;
		m_last->next.store(current);
		Advance();
		return true;
	}

	inline bool push(T &&item)
	{
		QueueNode* current = GetNewNode();
		current->value = std::move(item);
		m_last->next.store(current);
		Advance();
		return true;
	}

	inline bool try_pop(T &item)
	{
		QueueNode* currentlimit = m_limit.load();
		QueueNode* current = currentlimit->next.load();
		bool success = false;
		if (current != nullptr)
		{
			item = current->value;
			m_limit.store(current);
			success = true;
		}
		return success;
	}

	inline bool Empty()
	{
		QueueNode* currentlimit = m_limit.load();
		QueueNode* current = currentlimit->next.load();
		return current == nullptr;
	}
};

// One Producer - One Consumer lock free concurrent circular queue
template<typename T>
class CircularQueue
{
private:
	const size_t m_capacity;
	size_t increment(size_t idx) const
	{
		return (idx + 1) % m_capacity;
	}
	std::atomic<size_t>  m_tail;
	std::vector<T> m_container;
	std::atomic<size_t>  m_head;
public:
	CircularQueue(size_t capacity):
		m_tail(0),
		m_head(0),
		m_capacity(capacity)
	{
		m_container.resize(capacity);
	}

	CircularQueue():
		m_tail(0),
		m_head(0),
		m_capacity(128)
	{
		m_container.resize(m_capacity);
	}

	virtual ~CircularQueue()
	{}

	void resize(size_t capacity)
	{
		m_container.resize(m_capacity);
	}

	bool push(const T& item)
	{
		const size_t current_tail = m_tail.load(std::memory_order_relaxed);
		const size_t next_tail = increment(current_tail);
		if (next_tail != m_head.load(std::memory_order_acquire))
		{
			m_container[current_tail] = item;
			m_tail.store(next_tail, std::memory_order_release);
			return true;
		}
		return false;
	}
	bool push(T&& item)
	{
		const size_t current_tail = m_tail.load(std::memory_order_relaxed);
		const size_t next_tail = increment(current_tail);
		if (next_tail != m_head.load(std::memory_order_acquire))
		{
			m_container[current_tail] = std::move(item);
			m_tail.store(next_tail, std::memory_order_release);
			return true;
		}
		return false;
	}
	bool try_pop(T& item)
	{
		const size_t current_head = m_head.load(std::memory_order_relaxed);
		if (current_head == m_tail.load(std::memory_order_acquire))
			return false; // empty queue

		item = m_container[current_head];
		m_head.store(increment(current_head), std::memory_order_release);
		return true;
	}

	bool Empty() const
	{
		return (m_head.load() == m_tail.load());
	}

	bool Full() const
	{
		const auto next_tail = increment(m_tail.load());
		return (next_tail == m_head.load());
	}
};

// One Producer - Multiple Consumers
template <typename T, class Container = OneToOneQueue<T>, bool ContentionControl = true>
struct OneToManyQueue
{
private:
	SpinLock<ContentionControl> m_dequeueLock;
	Container m_inner;
public:
	OneToManyQueue(): m_inner(), m_dequeueLock()
	{}
	OneToManyQueue(size_t capacity): m_inner(capacity), m_dequeueLock()
	{}
	~OneToManyQueue()
	{}

	bool push(const T &item)
	{
		return m_inner.push(item);
	}

	bool push(T &&item)
	{
		return m_inner.push(std::move(item));
	}

	bool try_pop(T &item)
	{
		m_dequeueLock.lock();
		bool success = m_inner.try_pop(item);
		m_dequeueLock.unlock();
		return success;
	}

	bool Empty()
	{
		return m_inner.Empty();
	}
};

// Multiple Producer - One Consumers
template <typename T, class Container = OneToOneQueue<T>, bool ContentionControl = true>
struct ManyToOneQueue
{
private:
	SpinLock<ContentionControl> m_equeueLock;
	Container m_inner;
public:
	ManyToOneQueue(): m_inner(), m_equeueLock()
	{}
	ManyToOneQueue(size_t capacity): m_inner(capacity), m_equeueLock()
	{}
	~ManyToOneQueue()
	{}

	bool push(const T &item)
	{
		m_equeueLock.lock();
		bool success = m_inner.push(item);
		m_equeueLock.unlock();
		return success;
	}
	bool push(T &&item)
	{
		m_equeueLock.lock();
		bool success = m_inner.push(std::move(item));
		m_equeueLock.unlock();
		return success;
	}
	bool try_pop(T &item)
	{
		return m_inner.try_pop(item);
	}
	bool Empty()
	{
		return m_inner.Empty();
	}
};

// Multiple Producer - Multiple Consumers
template <typename T, class Container = OneToOneQueue<T>, bool ContentionControl = true>
struct ManyToManyQueue
{
private:
	SpinLock<ContentionControl> m_dequeueLock;
	SpinLock<ContentionControl> m_equeueLock;
	Container m_inner;
public:
	ManyToManyQueue():
		m_inner(),
		m_dequeueLock(),
		m_equeueLock()
	{

	}
	ManyToManyQueue(size_t capacity):
		m_inner(capacity),
		m_dequeueLock(),
		m_equeueLock()
	{}

	~ManyToManyQueue()
	{}

	bool push(const T &item)
	{
		m_equeueLock.lock();
		bool success = m_inner.push(item);
		m_equeueLock.unlock();
		return success;
	}

	bool push(T &&item)
	{
		m_equeueLock.lock();
		bool success = m_inner.push(std::move(item));
		m_equeueLock.unlock();
		return success;
	}

	bool try_pop(T &item)
	{
		m_dequeueLock.lock();
		bool success = m_inner.try_pop(item);
		m_dequeueLock.unlock();
		return success;
	}

	bool Empty()
	{
		return m_inner.Empty();
	}
};

class IWorker
{
public:
	virtual ~IWorker()
	{}
	virtual bool NextTask() = 0;
};

class ThreadPool
{
private:
	std::vector<std::unique_ptr<std::thread>> m_workerThreads;
	std::vector<IWorker*> m_workers;
	std::atomic<s32> m_workflag;
	std::atomic<s32> m_workercount;
	std::atomic<bool> m_working;
	static void Workloop(ThreadPool &state, size_t ID);
	static ThreadPool &Getinstance();
	ThreadPool(ThreadPool const&);
	void operator=(ThreadPool const&);
	ThreadPool();
public:
	virtual ~ThreadPool();
	static void NotifyWorkPending();
	static void RegisterWorker(IWorker* worker);
	static void UnregisterWorker(IWorker* worker);
};

class AsyncWorker final: IWorker
{
private:
	std::atomic<s32> m_inputsize;
	ManyToManyQueue<std::function<void()>, CircularQueue<std::function<void()>>> m_TaskQueue;
	static AsyncWorker &Getinstance();
	AsyncWorker();
public:
	virtual ~AsyncWorker();
	bool NextTask() override;
	static void ExecuteAsync(std::function<void()> &&func);
};
}
