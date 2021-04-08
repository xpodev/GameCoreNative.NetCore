
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
using GameConnection = Connection<GameMessage, ASIOAsyncUDPSocket>;

GameMessage msg;


struct ServerConnection : public GameConnection {
public:
	ServerConnection(ASIO_UDP& socket, ThreadSafeQueue<GameMessage>& queue)
		: m_inQueue(queue)
		, GameConnection(socket)
	{

	}

	void on_receive(GameMessage& msg) override {
		std::cout << "Message from: " << this->remote_endpoint() << std::endl;
		m_inQueue.push_back(msg);
	}

	ServerConnection(ServerConnection && sc)
		: ServerConnection(sc.m_socket, sc.m_inQueue)
	{}

	ThreadSafeQueue<GameMessage>& incoming() {
		return m_inQueue;
	}

private:
	ThreadSafeQueue<GameMessage>& m_inQueue;
};


void protocol_core(ThreadSafeQueue<GameMessage>& q) {
	while (true)
	{
		q.wait();
		GameMessage msg = q.pop_front();
	}
}


int main() {
	ThreadSafeQueue<GameMessage> q;
	std::vector<ServerConnection> conns;
	asio::io_context ctx{};
	asio::ip::udp::socket socket{ ctx, asio::ip::udp::endpoint(asio::ip::udp::v4(), 3741) };
	ServerConnection conn{ socket, q };
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
