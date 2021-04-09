#pragma once

#include "./IMessage.h"


namespace xpo {
	namespace net {
		template <class DataType, IMessage<DataType> M, class = void>
		struct SerializeType
		{
			void write(M& msg, DataType const& data);
			void read(M& msg, DataType& data);
		};

		template <class T, class MessageT>
		concept Serializable = requires (T & s, MessageT msg) {
			requires IMessage<MessageT, T>;
			SerializeType<T, MessageT>::write(msg, s);
			SerializeType<T, MessageT>::read(msg, s);
		};

		template <class T>
		requires std::is_enum_v<T>
			struct MessageHeader
		{
			size_t m_size;
			T m_id;

			typedef T commands;

			size_t size() {
				return m_size;
			}
		};

		template <IMessageHeader T>
		struct MessageBase {
			T header{};
			std::vector<uint8_t> m_body;

			typedef T header_type;

			uint8_t* data() {
				return m_body.data();
			}

			void add_data(uint8_t* data, size_t length) {
				size_t lastSize = m_body.size();
				m_body.resize(lastSize + length);
				std::memcpy(m_body.data() + lastSize, data, length);
			}

			void clear() {
				m_body.clear();
			}

			template <class DataType>
			friend MessageBase<T>& operator << (MessageBase<T>& msg, DataType const& data) {
				SerializeType<DataType, MessageBase<T>>::write(msg, data);
				return msg;
			}

			template <class DataType>
			friend MessageBase<T>& operator >> (MessageBase<T>& msg, DataType& data) {
				SerializeType<DataType, MessageBase<T>>::read(msg, data);
				return msg;
			}

			friend std::ostream& operator << (std::ostream& os, MessageBase<T> const& msg) {
				os << "ID:" << int(msg.header.m_id) << " Size:" << msg.header.m_size;
				return os;
			}
		};

		template <class T>
		requires std::is_enum_v<T>
			using Message = MessageBase<MessageHeader<T>>;

#ifndef GAMECORE_NET_OVERRIDE_DEFAULT_SERIALIZER_IMPLEMENTATION

		// Standard layout object serialization implementation
		template <class DataType, IMessageHeader H>
		struct SerializeType<DataType, MessageBase<H>, std::enable_if_t<std::is_standard_layout_v<DataType>>>
		{
			static void write(MessageBase<H>& msg, DataType const& data) {
				//static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to poped from vector");
				size_t i = msg.m_body.size();
				msg.m_body.resize(i + sizeof(DataType));

				std::memcpy(msg.m_body.data() + i, &data, sizeof(DataType));

				msg.header.m_size = msg.m_body.size();
			}

			static void read(MessageBase<H>& msg, DataType& data) {
				//static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to poped from vector");
				size_t i = msg.m_body.size() - sizeof(DataType);

				std::memcpy(&data, msg.m_body.data() + i, sizeof(DataType));

				msg.m_body.resize(i);

				msg.header.m_size = i;
			}
		};

		// String serialization implementation
		template <IMessageHeader H>
		struct SerializeType<std::string, MessageBase<H>> {
			static void write(MessageBase<H>& msg, std::string const& data) {
				for (char c : data) {
					msg << c;
				}
				int length = data.size();;
				msg << length;
			}

			static void read(MessageBase<H>& msg, std::string& data) {
				int length;
				char c;
				msg >> length;
				for (int i = 0; i < length; i++) {
					msg >> c;
					data.insert(data.begin(), c);
				}
			}
		};

		// Vector serialization implementation
		template <class DataType, IMessageHeader H>
		struct SerializeType<std::vector<DataType>, MessageBase<H>> {
			static void write(MessageBase<H>& msg, std::vector<DataType> const& data) {
				int length = data.size();
				for (int i = 0; i < length; ++i) {
					DataType temp = data[i];
					msg << temp;
				}
				msg << length;
			}

			static void read(MessageBase<H>& msg, std::vector<DataType>& data) {
				int length;
				msg >> length;
				for (int i = 0; i < length; ++i) {
					DataType temp;
					msg >> temp;
					data.emplace(data.begin(), temp);
				}
			}
		};

		// Serializable serialization implementation
		template <class DataType, IMessageHeader H>
		requires Serializable<DataType, MessageBase<H>>
			struct SerializeType<DataType, MessageBase<H>>
		{
			static void write(MessageBase<H>& msg, DataType const& data) {
				DataType::write(msg, data);
			}

			static void read(MessageBase<H>& msg, DataType& data) {
				DataType::read(msg, data);
			}
		};

#endif // !GAMECORE_NET_OVERRIDE_DEFAULT_SERIALIZER_IMPLEMENTATION
	}
}
