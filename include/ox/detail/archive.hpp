#pragma once
#include "connection.hpp"
#include "util.hpp"
#include <boost/cast.hpp>
#include <boost/optional.hpp>
#include <cereal/cereal.hpp>
#include <cereal/types/tuple.hpp>
#include <exception>
#include <type_traits>

namespace ox
{
	namespace detail
	{
		class oarchive : public cereal::OutputArchive<oarchive>
		{
		public:
			oarchive(std::ostream& os, const std::shared_ptr<connection>& connection)
				: cereal::OutputArchive<oarchive>(this)
				, os_(os)
				, connection_(connection)
			{
			}

			void save_binary(const char* data, std::size_t size)
			{
				os_.write(data, size);
			}

			template <class Value>
			typename std::enable_if<std::is_arithmetic<Value>::value>::type
			save_integer(const Value& value)
			{
				save_binary(reinterpret_cast<const char*>(&value), sizeof(value));
			}

			void save_string(const std::string& value)
			{
				std::uint64_t size = value.size();
				save_integer(size);
				save_binary(value.data(), value.size());
			}

			template <class... Arguments>
			void save_function(const std::function<void(Arguments...)>& value);

		private:
			std::ostream& os_;
			std::shared_ptr<connection> connection_;
		};

		class iarchive : public cereal::InputArchive<iarchive>
		{
		public:
			iarchive(std::istream& is, const std::shared_ptr<connection>& connection)
				: cereal::InputArchive<iarchive>(this)
				, is_(is)
				, connection_(connection)
			{
			}

			void load_binary(char* data, std::size_t size)
			{
				is_.read(data, size);
			}

			template <class Value>
			typename std::enable_if<std::is_arithmetic<Value>::value>::type
			load_integer(Value& value)
			{
				load_binary(reinterpret_cast<char*>(&value), sizeof(value));
			}

			void load_string(std::string& value)
			{
				std::uint64_t size;
				load_integer(size);

				value.resize(boost::numeric_cast<std::size_t>(size));
				load_binary(&value.front(), value.size());
			}

			template <class... Arguments>
			void load_function(std::function<void(Arguments...)>& value);

		private:
			std::istream& is_;
			std::shared_ptr<connection> connection_;
		};

		template <class... Arguments>
		void oarchive::save_function(const std::function<void(Arguments...)>& value)
		{
			auto c = connection_;

			auto id = connection_->resgister_callback([value, c](const auto& str) {
				std::istringstream is(str);
				iarchive ia(is, c);

				std::tuple<std::decay_t<Arguments>...> args;
				ia(args);

				try
				{
					apply(value, args);
				}
				catch (...)
				{
				}
			});

			save_integer(id);
		}

		template <class... Arguments>
		void iarchive::load_function(std::function<void(Arguments...)>& value)
		{
			auto c = connection_;

			std::uint64_t id;
			load_integer(id);

			auto deleter = std::shared_ptr<void>(nullptr, [c, id](auto) {
				c->unregister_callback_remote(id);
			});

			value = [c, id, deleter](Arguments... args) {
				std::ostringstream os;

				{
					oarchive oa(os, c);
					oa(std::make_tuple(args...));
				}

				c->invoke_remote(id, os.str());
			};
		}

		template <class Value>
		typename std::enable_if<std::is_arithmetic<Value>::value>::type
		CEREAL_SAVE_FUNCTION_NAME(oarchive& ar, const Value& value)
		{
			ar.save_integer(value);
		}

		inline void CEREAL_SAVE_FUNCTION_NAME(oarchive& ar, const std::string& value)
		{
			ar.save_string(value);
		}

		template <class Result, class... Arguments>
		void CEREAL_SAVE_FUNCTION_NAME(oarchive& ar, const std::function<Result(Arguments...)>& value)
		{
			static_assert(std::is_void<Result>::value, "It is not allowed to use return value since it is difficult to gurantee returning");
			ar.save_function(value);
		}

		template <class Value>
		void CEREAL_SAVE_FUNCTION_NAME(oarchive& ar, const cereal::NameValuePair<Value>& value)
		{
			ar(value.value);
		}

		template <class Value>
		typename std::enable_if<std::is_arithmetic<Value>::value>::type
		CEREAL_LOAD_FUNCTION_NAME(iarchive& ar, Value& value)
		{
			ar.load_integer(value);
		}

		inline void CEREAL_LOAD_FUNCTION_NAME(iarchive& ar, std::string& value)
		{
			ar.load_string(value);
		}

		template <class Result, class... Arguments>
		void CEREAL_LOAD_FUNCTION_NAME(iarchive& ar, std::function<Result(Arguments...)>& value)
		{
			static_assert(std::is_void<Result>::value, "It is not allowed to use return value since it is difficult to gurantee returning");
			ar.load_function(value);
		}

		template <class Value>
		void CEREAL_LOAD_FUNCTION_NAME(iarchive& ar, cereal::NameValuePair<Value>& value)
		{
			ar(value.value);
		}
	}
}

CEREAL_REGISTER_ARCHIVE(ox::detail::oarchive)
CEREAL_REGISTER_ARCHIVE(ox::detail::iarchive)
