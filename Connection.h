#pragma once

#include "./ASIOSocket.h"
#include "./IAsyncIO.h"
#include "./IMessage.h"
#include "./IQueue.h"
#include "./ThreadSafeQueue.h"

#ifndef CONNECTION_UDP_DEFAULT_BUFFER_SIZE
#define CONNECTION_UDP_DEFAULT_BUFFER_SIZE 512
#endif


namespace xpo {
	namespace net {
		template <class T, class M>
		concept IMessageProcessor = requires (T proc, M& msg, std::error_code ec) {
			requires IByteMessage<M>;
			proc.on_receive(msg);
			proc.on_send(msg);
			{ proc.on_receive_fail(ec) } -> std::same_as<bool>; // return value indicates whether to continue listening.
			{ proc.on_send_fail(ec) } -> std::same_as<bool>; // return value indicates whether to continue listening.
		};

		template <class T, class M>
		concept ITCPMessageProcessor = requires (T proc, M & msg, std::error_code ec) {
			requires IMessageProcessor<T, M>;
			{ proc.on_receive_header(msg.header) } -> std::same_as<bool>; // return value indicates whether to drop the packet.
		};

		template <class T, class M>
		concept IUDPMessageProcessor = requires (T proc, M & msg, std::error_code ec) {
			requires IMessageProcessor<T, M>;
		};

		template <IByteMessage T>
		struct OwnedMessage : public T
		{
			OwnedMessage()
				: T()
				, m_endPoint()
			{

			}

			OwnedMessage(T& msg, asio::ip::udp::endpoint const& endPoint)
				: T(msg)
				, m_endPoint(endPoint)
			{

			}

			asio::ip::udp::endpoint& endpoint() {
				return m_endPoint;
			}

		private:
			asio::ip::udp::endpoint m_endPoint;
		};

		template <IByteMessage T>
		struct TCPMessageProcessor {
			uint32_t const MAX_MESSAGE_BODY_SIZE = 1024;

			virtual void on_receive(T& msg) {
				std::cout << "Received: " << msg << std::endl;
			}

			bool on_receive_header(typename T::header_type& header) {
				// if the header says the message is too large, just drop it
				if (header.size() > MAX_MESSAGE_BODY_SIZE) {
					std::cout << "Message body size was too large: " << header.size() << std::endl;
					return false;
				}
				return true;
			}

			virtual void on_send(T& msg) {
				std::cout << "Sending: " << msg << std::endl;
			}

			virtual bool on_receive_fail(std::error_code ec) {
				std::cout << "Receive Failed: " << ec.message() << std::endl;
				return true;
			}

			virtual bool on_send_fail(std::error_code ec) {
				std::cout << "Send Failed: " << ec.message() << std::endl;
				return true;
			}
		};

		template <IByteMessage T>
		struct UDPMessageProcessor {
			virtual void on_send(OwnedMessage<T>& msg) {
				std::cout << "Sending: " << msg << std::endl;
			}

			virtual void on_receive(T& msg) {
				std::cout << "Received: " << msg << std::endl;
			}

			virtual bool on_receive_fail(std::error_code ec) {
				std::cout << "Receive Failed: " << ec.message() << std::endl;
				return true;
			}

			virtual bool on_send_fail(std::error_code ec) {
				std::cout << "Send Failed: " << ec.message() << std::endl;
				return true;
			}
		};

		template <
			IByteMessage T,
			std::derived_from<IAsyncByteIO> AsyncT = IAsyncByteIO,
			IMessageProcessor<T> P = UDPMessageProcessor<T>,
			IQueue<T> Q = ThreadSafeQueue<T>
		>
		struct ConnectionBase : public P, public AsyncT {
		public:
			using AsyncT::AsyncT;

		public:
			void send_message(T const& msg) {
				this->execute_async([this, msg]() {
					send_message_async(msg);
				});
			}

			void listen_for_messages() {
				this->execute_async([this]() {
					begin_receive_async();
				});
			}

		protected:
			void send_message_async(T const& msg) {
				m_outQueue.push_back(msg);
				if (m_outQueue.size() == 1) {
					begin_send_async();
				}
			}

			virtual void begin_receive_async() = 0;

			virtual void begin_send_async() = 0;

		protected:
			T m_tempInMessage;
			T m_tempOutMessage;
			Q m_outQueue;
		};

		template <IByteMessage T>
		struct TCPConnection : public ConnectionBase<T, ASIOAsyncTCPSocket, TCPMessageProcessor<T>, ThreadSafeQueue<T>> {
			using ConnectionBase<T, ASIOAsyncTCPSocket, TCPMessageProcessor<T>, ThreadSafeQueue<T>>::ConnectionBase;

			void begin_receive_async() override {
				header_receive_async();
			}

			void header_receive_async() {
				this->read_async((uint8_t*)(&this->m_tempInMessage.header), sizeof(this->m_tempInMessage.header), [=](std::error_code ec, size_t length) {
					if (!ec && length == sizeof(this->m_tempInMessage.header)) {
						if (this->on_receive_header(this->m_tempInMessage.header)) {
							if (this->m_tempInMessage.header.size() > 0) {
								body_receive_async();
							}
							else {
								this->on_receive(this->m_tempInMessage);
								header_receive_async();
							}
						}
						else {
							header_receive_async();
						}
					}
					else {
						if (this->on_receive_fail(ec)) {
							header_receive_async();
						}
					}
					});
			}

			void body_receive_async() {
				// TODO: there's got to be a better way than using the 'new' operator
				uint8_t* buffer = new uint8_t[this->m_tempInMessage.header.size()];
				this->read_async(buffer, this->m_tempInMessage.header.size(), [this, buffer](std::error_code ec, size_t length) {
					this->m_tempInMessage.data(buffer, length);
					// Here we delete the allocated memory
					delete[] buffer;
					if (!ec && length == this->m_tempInMessage.header.size()) {
						this->on_receive(this->m_tempInMessage);
						header_receive_async();
					}
					else {
						if (this->on_receive_fail(ec)) {
							header_receive_async();
						}
					}
					});
			}

			void begin_send_async() override {
				header_send_async();
			}

			void header_send_async() {
				this->m_tempOutMessage = this->m_outQueue.pop_front();
				this->on_send(this->m_tempOutMessage);
				this->write_async((uint8_t*)(&this->m_tempOutMessage.header), sizeof(this->m_tempOutMessage.header), [this](std::error_code ec, size_t length) {
					if (!ec && length == sizeof(this->m_tempOutMessage.header)) {
						if (this->m_tempOutMessage.header.size() > 0) {
							body_send_async();
						}
						else {
							if (!this->m_outQueue.empty()) {
								header_send_async();
							}
						}
					}
					else {
						if (this->on_send_fail(ec)) {
							if (!this->m_outQueue.empty()) {
									header_send_async();
								}
						}
					}
					});
			}

			void body_send_async() {
				this->write_async(this->m_tempOutMessage.data(), this->m_tempOutMessage.header.size(), [this](std::error_code ec, size_t length) {
					if (!ec && length == this->m_tempOutMessage.header.size()) {
						if (!this->m_outQueue.empty()) {
							header_send_async();
						}
					}
					else {
						if (this->on_send_fail(ec) && !this->m_outQueue.empty()) {
							header_send_async();
						}
					}
					});
			}
		};

		template <IByteMessage T>
		struct UDPConnection : public ConnectionBase<OwnedMessage<T>, ASIOAsyncUDPSocket, UDPMessageProcessor<T>, ThreadSafeQueue<OwnedMessage<T>>> {
			using ConnectionBase<OwnedMessage<T>, ASIOAsyncUDPSocket, UDPMessageProcessor<T>, ThreadSafeQueue<OwnedMessage<T>>>::ConnectionBase;

			void send_message_to(T const& msg, asio::ip::udp::endpoint& endPoint) {
				send_message(OwnedMessage(msg, endPoint));
			}

			size_t in_buffer_size() const {
				return m_inBufferSize;
			}

			void in_buffer_size(size_t size) {
				m_inBufferSize = size;
			}

			size_t out_buffer_size() const {
				return m_inBufferSize;
			}

			void out_buffer_size(size_t size) {
				m_outBufferSize = size;
			}

			void free_in_buffer() {
				delete[] m_inBuffer;
			}

			void free_out_buffer() {
				delete[] m_inBuffer;
			}

		protected:
			void begin_send_async() override {
				if (m_outBuffer == nullptr) {
					m_outBuffer = new uint8_t[m_outBufferSize];
				}
				message_send_async();
			}

			void begin_receive_async() override {
				if (m_inBuffer == nullptr) {
					m_inBuffer = new uint8_t[m_inBufferSize];
				}
				message_receive_async();
			}

			void message_send_async() {
				this->m_tempOutMessage = this->m_outQueue.pop_front();
				this->m_remoteOutEndPoint = this->m_tempOutMessage.endpoint();
				this->on_send(this->m_tempOutMessage);
				std::memcpy(m_outBuffer, &this->m_tempOutMessage.header, sizeof(this->m_tempOutMessage.header));
				std::memcpy(m_outBuffer + sizeof(this->m_tempOutMessage.header), this->m_tempOutMessage.data(), this->m_tempOutMessage.header.size());
				this->write_async(m_outBuffer, sizeof(this->m_tempOutMessage.header) + this->m_tempOutMessage.header.size(), [this](std::error_code ec, size_t length) {
					if (!ec) {
						if (this->m_outQueue.size() > 0) {
							message_send_async();
						}
					}
					else {
						if (this->on_send_fail(ec)) {
							if (this->m_outQueue.size() > 0) {
								message_send_async();
							}
						}
					}
				});
			}

			void message_receive_async() {
				this->read_async(m_inBuffer, m_inBufferSize, [this](std::error_code ec, size_t length) {
					if (!ec) {
						parse_message_from_byte_stream(length);
						message_receive_async();
					}
					else {
						if (this->on_receive_fail(ec)) {
							message_receive_async();
						}
					}
				});
			}

			void parse_message_from_byte_stream(size_t bytesReceived) {
				// otherwise, try to parse a new message
				// we should pasre the whole buffer, since it is being overwriten every time we receive

				uint8_t* begin = m_inBuffer;
				uint8_t* end = begin + bytesReceived;
				// we are not parsing a message write now, lets parse a new one
				while (begin != end) {
					if (m_remainingBytesForCurrentMessage == 0) {
						this->m_tempInMessage.clear();
						std::memcpy(&this->m_tempInMessage.header, begin, sizeof(this->m_tempInMessage.header));
						begin += sizeof(this->m_tempInMessage.header);
						m_remainingBytesForCurrentMessage = this->m_tempInMessage.header.size();
					}
					uint8_t* endOfMessageBuffer = std::min(end, begin + m_remainingBytesForCurrentMessage);
					size_t count = endOfMessageBuffer - begin;
					this->m_tempInMessage.add_data(begin, count);
					m_remainingBytesForCurrentMessage -= count;

					if (m_remainingBytesForCurrentMessage == 0) {
						this->on_receive(this->m_tempInMessage);
					}

					begin += count;
				}
			}

			size_t m_inBufferSize = CONNECTION_UDP_DEFAULT_BUFFER_SIZE;
			uint8_t* m_inBuffer = nullptr;

			size_t m_remainingBytesForCurrentMessage = 0;
			size_t m_inBufferOffset;

			size_t m_outBufferSize = CONNECTION_UDP_DEFAULT_BUFFER_SIZE;
			uint8_t* m_outBuffer = nullptr;
		};
	}
}
