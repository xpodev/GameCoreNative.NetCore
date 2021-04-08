#pragma once

#include "./ASIOSocket.h"
#include "./IAsyncIO.h"
#include "./IQueue.h"
#include "./ThreadSafeQueue.h"


namespace xpo {
	namespace net {
		template <class T, class M>
		concept MessageProcessor = requires (T proc, M& msg, std::error_code ec) {
			requires IByteMessage<M>;
			proc.on_receive(msg);
			{ proc.on_receive_header(msg.header) } -> std::same_as<bool>; // return value indicates whether to drop the packet.
			proc.on_send(msg);
			{ proc.on_receive_fail(ec) } -> std::same_as<bool>; // return value indicates whether to continue listening.
			{ proc.on_send_fail(ec) } -> std::same_as<bool>; // return value indicates whether to continue listening.
		};

		template <IByteMessage T>
		struct DefualtMessageProcessor {
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
		struct SentMessage : public T
		{
			SentMessage()
				: T()
				, m_endPoint()
			{

			}

			SentMessage(T& msg, asio::ip::udp::endpoint const& endPoint)
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
		struct DefualtUDPMessageProcessor : public DefualtMessageProcessor<SentMessage<T>> {
			uint32_t const MAX_MESSAGE_BODY_SIZE = 1024;

			virtual void on_send(SentMessage<T>& msg) {
				std::cout << "Sending: " << msg << std::endl;
			}

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
			MessageProcessor<T> P = DefualtMessageProcessor<T>,
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
					header_receive_async();
				});
			}

		protected:
			void send_message_async(T const& msg) {
				m_outQueue.push_back(msg);
				if (m_outQueue.size() == 1) {
					header_send_async();
				}
			}

			virtual void header_receive_async() {
				this->read_async((uint8_t*)(&m_tempInMessage.header), sizeof(m_tempInMessage.header), [=](std::error_code ec, size_t length) {
					if (!ec && length == sizeof(m_tempInMessage.header)) {
						if (this->on_receive_header(m_tempInMessage.header)) {
							if (m_tempInMessage.header.size() > 0) {
								body_receive_async();
							}
							else {
								this->on_receive(m_tempInMessage);
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

			virtual void body_receive_async() {
				// TODO: there's got to be a better way than using the 'new' operator
				uint8_t* buffer = new uint8_t[m_tempInMessage.header.size()];
				this->read_async(buffer, m_tempInMessage.header.size(), [this, buffer](std::error_code ec, size_t length) {
					m_tempInMessage.data(buffer, length);
					// Here we delete the allocated memory
					delete[] buffer;
					if (!ec && length == m_tempInMessage.header.size()) {
						this->on_receive(m_tempInMessage);
						header_receive_async();
					}
					else {
						if (this->on_receive_fail(ec)) {
							header_receive_async();
						}
					}
				});
			}

			virtual void header_send_async() {
				m_tempOutMessage = m_outQueue.pop_front();
				this->on_send(m_tempOutMessage);
				this->write_async((uint8_t*)(&m_tempOutMessage.header), sizeof(m_tempOutMessage.header), [this](std::error_code ec, size_t length) {
					if (!ec && length == sizeof(m_tempOutMessage.header)) {
						if (m_tempOutMessage.header.size() > 0) {
							body_send_async();
						}
						else {
							if (!m_outQueue.empty()) {
								header_send_async();
							}
						}
					}
					else {
						if (this->on_send_fail(ec)) {
							if (m_tempOutMessage.header.size() > 0) {
								body_send_async();
							}
							else {
								if (!m_outQueue.empty()) {
									header_send_async();
								}
							}
						}
					}
					});
			}

			virtual void body_send_async() {
				this->write_async(m_tempOutMessage.data(), m_tempOutMessage.header.size(), [this](std::error_code ec, size_t length) {
					if (!ec && length == m_tempOutMessage.header.size()) {
						if (!m_outQueue.empty()) {
							header_send_async();
						}
					}
					else {
						if (this->on_send_fail(ec) && !m_outQueue.empty()) {
							header_send_async();
						}
					}
					});
			}

		protected:
			T m_tempInMessage;
			T m_tempOutMessage;
			Q m_outQueue;
		};

		template <IByteMessage T>
		struct TCPConnection : public ConnectionBase<T, ASIOAsyncTCPSocket, DefualtMessageProcessor<T>, ThreadSafeQueue<T>> {
			using ConnectionBase<T, ASIOAsyncTCPSocket, DefualtMessageProcessor<T>, ThreadSafeQueue<T>>::ConnectionBase;
		};

		template <IByteMessage T>
		struct UDPConnection : public ConnectionBase<SentMessage<T>, ASIOAsyncUDPSocket, DefualtUDPMessageProcessor<T>, ThreadSafeQueue<SentMessage<T>>> {
			using ConnectionBase<SentMessage<T>, ASIOAsyncUDPSocket, DefualtUDPMessageProcessor<T>, ThreadSafeQueue<SentMessage<T>>>::ConnectionBase;

			void header_send_async() override {
				this->m_tempOutMessage = this->m_outQueue.front();
				this->m_remoteOutEndPoint = this->m_tempOutMessage.endpoint();
				ConnectionBase<SentMessage<T>, ASIOAsyncUDPSocket, DefualtUDPMessageProcessor<T>, ThreadSafeQueue<SentMessage<T>>>::header_send_async();
			}

			void send_message_to(T const& msg, asio::ip::udp::endpoint& endPoint) {
				this->send_message(SentMessage(msg, endPoint));
			}
		};
	}
}
