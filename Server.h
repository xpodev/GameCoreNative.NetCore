#pragma once

#include "./IConnection.h"


namespace xpo {
	namespace Net {
		template <IByteMessage M, IConnection<M> T>
		struct SingleSocketServer {
			void start() {
				
			}
		protected:
			T m_connection;
		};
	}
}
