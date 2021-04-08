#pragma once

#include <type_traits>
#include <concepts>


namespace xpo {
	namespace Net {
		template <class T, class Ty>
		concept IQueue = requires (T q, Ty & v, Ty const& vc) {
			{ q.front() } -> std::same_as<Ty const&>;
			q.push_front(v);
			{ q.empty() } -> std::same_as<bool>;
			{ q.size() } -> std::same_as<std::size_t>;
			q.clear();
			{ q.pop_front() } -> std::same_as<Ty>;
		};

		template <class T, class Ty>
		concept IDeque = requires (T q, Ty & v, Ty const& vc) {
			requires IQueue<T, Ty>;
			{ q.back() } -> std::same_as<Ty const&>;
			q.push_back(v);
			{ q.pop_back() } -> std::same_as<Ty>;
		};
	}
}
