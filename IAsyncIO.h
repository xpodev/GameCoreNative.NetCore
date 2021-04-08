#pragma once

#include <stdint.h>
#include <functional>
#include <system_error>

namespace xpo {
	namespace Net {
		template <class IOType>
		struct IAsyncIO {
		public:
			virtual void execute_async(std::function<void()> f) = 0;

			virtual void read_async(IOType* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) = 0;

			virtual void write_async(IOType* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) = 0;

			virtual void close() = 0;

			virtual bool is_open() = 0;
		};

		using IAsyncByteIO = IAsyncIO<uint8_t>;
	}
}