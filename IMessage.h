#pragma once

#include <type_traits>
#include <string>
#include <vector>
#include <iostream>


namespace xpo {
	namespace net {
		template <class T>
		concept IMessageHeader = requires (T hdr) {
			{ hdr.size() } -> std::convertible_to<std::size_t>; // returns the size of the body
			{ sizeof(hdr) } -> std::convertible_to<std::size_t>; // returns the size of the header (constexpr)
			{ (uint8_t*)(&hdr) }; // convert the header to byte*
			{ (T*)(std::declval<uint8_t*>()) }; // convert byte* to header
		};

		template <class T>
		concept IByteMessage = requires (T& msg) {
			typename T::header_type;
			{ msg.header } -> std::convertible_to<typename T::header_type>;
			requires IMessageHeader<typename T::header_type>;
			{ msg.data() } -> std::convertible_to<uint8_t*>;
			msg.add_data(std::declval<uint8_t*>(), std::declval<std::size_t>());
			msg.clear();
		};

		template <class T, class V>
		concept IMessageIO = requires (T msg, V & v, V const& vc) {
			{ msg << vc } -> std::same_as<T&>;
			{ msg >> v } -> std::same_as<T&>;
		};

		template <class T, class V>
		concept IMessage = requires {
			requires IByteMessage<T>;
			requires IMessageIO<T, V>;
		};
	}
}
