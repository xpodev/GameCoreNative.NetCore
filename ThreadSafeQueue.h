#pragma once

#include <deque>
#include <mutex>

#include "./IQueue.h"


namespace xpo {
	namespace Net {
		template <class T>
		class Deque : public std::deque<T> {
		public:
			Deque() = default;
			Deque(Deque<T> const&) = delete;
			virtual ~Deque() { clear(); }

			T const& front() {
				return std::deque<T>::front();
			}

			T const& back() {
				return std::deque<T>::back();
			}

			void push_front(T const& item) {
				std::deque<T>::emplace_front(item);
			}

			void push_back(T const& item) {
				std::deque<T>::emplace_back(item);
			}

			bool empty() {
				return std::deque<T>::empty();
			}

			size_t size() {
				return std::deque<T>::size();
			}

			void clear() {
				std::deque<T>::clear();
			}

			T pop_front() {
				auto t = std::move(std::deque<T>::front());
				std::deque<T>::pop_front();
				return t;
			}

			T pop_back() {
				auto t = std::move(std::deque<T>::back());
				std::deque<T>::pop_back();
				return t;
			}
		};

		template <class T, IDeque<T> Q = Deque<T>>
		class ThreadSafeQueue {
		public:
			ThreadSafeQueue() = default;
			ThreadSafeQueue(ThreadSafeQueue<T> const&) = delete;
			virtual ~ThreadSafeQueue() { clear(); }

			T const& front() {
				std::scoped_lock lock(m_mutex);
				return m_deque.front();
			}

			T const& back() {
				std::scoped_lock lock(m_mutex);
				return m_deque.back();
			}

			void push_front(T const& item) {
				std::scoped_lock lock(m_mutex);
				m_deque.push_back(item);

				std::unique_lock<std::mutex> ul(m_mutexBlocking);
				m_cvBlocking.notify_one();
			}

			void push_back(T const& item) {
				std::scoped_lock lock(m_mutex);
				m_deque.push_back(item);

				std::unique_lock<std::mutex> ul(m_mutexBlocking);
				m_cvBlocking.notify_one();
			}

			bool empty() {
				std::scoped_lock lock(m_mutex);
				return m_deque.empty();
			}

			size_t size() {
				std::scoped_lock lock(m_mutex);
				return m_deque.size();
			}

			void clear() {
				std::scoped_lock lock(m_mutex);
				return m_deque.clear();
			}

			T pop_front() {
				std::scoped_lock lock(m_mutex);
				auto t = std::move(m_deque.front());
				m_deque.pop_front();
				return t;
			}

			T pop_back() {
				std::scoped_lock lock(m_mutex);
				auto t = std::move(m_deque.back());
				m_deque.pop_back();
				return t;
			}

			void wait() {
				while (empty()) {
					std::unique_lock<std::mutex> ul(m_mutexBlocking);
					m_cvBlocking.wait(ul);
				}
			}

		protected:
			std::mutex m_mutex;
			Q m_deque;

			std::condition_variable m_cvBlocking;
			std::mutex m_mutexBlocking;
		};
	}
}
