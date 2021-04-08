#pragma once


namespace GameCore {
	template <class T, class U>
	requires std::is_same_v<T, U>
	void is_of_type(T t) {}

	template <class T>
	concept of_type = requires (T t) {
		is_of_type(t);
	};
}
