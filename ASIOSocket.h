#pragma once

#include <type_traits>

#include <asio/ts/net.hpp>

#include "./IAsyncIO.h"

namespace xpo {
	namespace net {
		template <class T>
		struct is_asio_socket : std::false_type {};

		using ASIO_TCP = asio::ip::tcp::socket;
		using ASIO_UDP = asio::ip::udp::socket;

		template <>
		struct is_asio_socket<ASIO_TCP> : std::true_type {};

		template <>
		struct is_asio_socket<ASIO_UDP> : std::true_type {};

		template <class T>
		constexpr bool is_asio_socket_v = is_asio_socket<T>::value;

		template <class T, typename = std::enable_if_t<is_asio_socket_v<T>>>
		struct ASIOAsyncSocketBase {
			
		};

		template <>
		struct ASIOAsyncSocketBase<ASIO_TCP> : public IAsyncIO<uint8_t> {
			ASIOAsyncSocketBase(ASIO_TCP& socket)
				: m_socket(std::move(socket))
			{}

			void read_async(uint8_t* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) override {
				asio::async_read(this->m_socket, asio::buffer(buffer, count), callback);
			}

			void write_async(uint8_t* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) override {
				asio::async_write(this->m_socket, asio::buffer(buffer, count), callback);
			}

			asio::ip::tcp::endpoint const& remote_endpoint() const {
				return m_socket.remote_endpoint();
			}

		protected:
			ASIO_TCP m_socket;
		};

		template <>
		struct ASIOAsyncSocketBase<ASIO_UDP> : public IAsyncByteIO {
			ASIOAsyncSocketBase(ASIO_UDP& socket)
				: m_socket(socket)
			{}

			void read_async(uint8_t* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) override {
				m_socket.async_receive_from(asio::buffer(buffer, count), m_remoteEndPoint, callback);
			}

			void write_async(uint8_t* const buffer, std::size_t count, std::function<void(std::error_code, std::size_t)> callback) override {
				m_socket.async_send_to(asio::buffer(buffer, count), m_remoteEndPoint, callback);
			}

			asio::ip::udp::endpoint const& remote_endpoint() const {
				return m_remoteEndPoint;
			}

		protected:
			asio::ip::udp::endpoint m_remoteEndPoint;
			ASIO_UDP& m_socket;
		};

		template <class T, typename = std::enable_if_t<is_asio_socket_v<T>>>
		struct ASIOAsyncSocket : public ASIOAsyncSocketBase<T> {
			using ASIOAsyncSocketBase<T>::ASIOAsyncSocketBase;

			void execute_async(std::function<void()> f) override {
				asio::post(this->m_socket.get_executor(), f);
			}

			void close() override {
				this->m_socket.close();
			}

			bool is_open() override {
				return this->m_socket.is_open();
			}

			T& socket() {
				return this->m_socket;
			}
		};

		using ASIOAsyncTCPSocket = ASIOAsyncSocket<ASIO_TCP>;
		using ASIOAsyncUDPSocket = ASIOAsyncSocket<ASIO_UDP>;
	}
}
