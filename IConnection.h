#pragma once

#include "./IMessage.h"


namespace xpo {
	namespace Net {
		template <class T, class M>
		concept IConnection = requires (T conn, M& msg) {
			requires IByteMessage<M>;
			conn.send_message(msg);
			conn.listen_for_messages();
			conn.close();
			{ conn.is_open() } -> std::same_as<bool>;
		};
	}
}
