
#ifdef _WIN32
#define _WIN32_WINNT 0x0A00
#endif

#define ASIO_STANDALONE

#include <thread>
#include <map>

#include <asio/ts/net.hpp>

#include "Message.h"
#include "Connection.h"
#include "ASIOSocket.h"



using namespace xpo::net;


enum class Commands {
	Chat = 100
};

template <class T>
requires std::is_enum_v<T>
using ProtocolHandlers = std::map < T, bool(Message<T>&)>;

using MyProtocol = ProtocolHandlers<Commands>;

using GameMessage = Message<Commands>;
//using GameConnection = ConnectionBase<OwnedMessage<GameMessage>, ASIOAsyncUDPSocket, DefualtUDPMessageProcessor<GameMessage>, ThreadSafeQueue<OwnedMessage<GameMessage>>>;
using GameConnection = UDPConnection<GameMessage>;


struct ServerConnection : public GameConnection {
public:
	ServerConnection(ASIO_UDP& socket, ThreadSafeQueue<OwnedMessage<GameMessage>>& queue)
		: m_inQueue(queue)
		, GameConnection(socket)
	{

	}

	void on_receive(GameMessage& msg) override {
		std::cout << "Message from: " << this->remote_endpoint() << std::endl;
		auto smsg = OwnedMessage<GameMessage>(msg, this->remote_endpoint());
		m_inQueue.push_back(smsg);
	}

	ServerConnection(ServerConnection && sc)
		: ServerConnection(sc.m_socket, sc.m_inQueue)
	{}

	ThreadSafeQueue<OwnedMessage<GameMessage>>& incoming() {
		return m_inQueue;
	}

private:
	ThreadSafeQueue<OwnedMessage<GameMessage>>& m_inQueue;
};


GameMessage msg;
ThreadSafeQueue<OwnedMessage<GameMessage>> q;
asio::io_context ctx = asio::io_context();
asio::ip::udp::socket sock = asio::ip::udp::socket{ ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), 3741) };
ServerConnection conn = ServerConnection{ sock, q };


void protocol_core(ThreadSafeQueue<OwnedMessage<GameMessage>>& q) {
	while (true)
	{
		q.wait();
		auto msg = q.pop_front();
		std::cout << "Sending " << msg <<  " to: " << msg.endpoint() << std::endl;
		conn.send_message(msg);
	}
}


int main() {
	msg.header.m_id = Commands::Chat;
	msg << std::string("Hello, World!");
	try {
		conn.listen_for_messages();
		std::thread asyncThread{ [&]() { ctx.run(); } };

		protocol_core(q);
	}
	catch (std::exception& e) {
		std::cout << "[SERVER] Exception: " << e.what() << std::endl;
		return false;
	}

	return 0;
}
