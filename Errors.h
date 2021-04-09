#pragma once

#include <system_error>


namespace xpo {
	namespace net {
		enum class ErrorCode {
			InvalidHeader = 1,
		};

		struct NetError : public std::error_category {
			virtual const char* name() const noexcept {
				return "XpoNet";
			}

			virtual std::string message(int ev) const noexcept {
				switch (static_cast<ErrorCode>(ev))
				{
				case ErrorCode::InvalidHeader:
					return "Invalid Header";
				default:
					break;
				}
			}

			virtual bool equivalent(const std::error_code& code, int condition) const noexcept {
				return false;
			}
		};

		NetError& net_error() {
			static NetError netError;
			return netError;
		}

		std::error_condition make_error_condition(ErrorCode e)
		{
			return std::error_condition(static_cast<int>(e), net_error());
		}

		std::error_code make_error_code(ErrorCode e) {
			return std::error_code(static_cast<int>(e), net_error());
		}
	}
}

namespace std
{
	template <>
	struct is_error_condition_enum<xpo::net::ErrorCode>
		: public true_type {};
}
