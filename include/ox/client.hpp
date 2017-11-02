#pragma once
#include "detail/archive.hpp"
#include "detail/connection.hpp"
#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <condition_variable>
#include <memory>
#include <string>
#include <thread>

namespace ox
{
	template <class Function>
	class client;

	template <class Result, class... Arguments>
	class client<Result(Arguments...)>
	{
	public:
		static_assert(std::is_void<Result>::value, "It is not allowed to use return value since it is difficult to gurantee returning");

		typedef std::function<void(Arguments...)> function_type;

		explicit client(const char* host, unsigned short port = 21872)
			: host_(host)
			, port_(port)
		{
			thread_ = std::thread(std::bind(&client::run, this));
		}

		~client()
		{
			io_service_.stop();
			thread_.join();
		}

		void operator()(Arguments... args)
		{
			boost::asio::ip::tcp::resolver::query query(host_, boost::lexical_cast<std::string>(port_));

			resolver_.async_resolve(query, [=](const auto& ec, auto it) {
				if (ec)
					return;

				auto c = std::make_shared<detail::connection>(io_service_);

				c->socket().async_connect(it->endpoint(), [=](const auto& ec) {
					if (ec)
						return;

					c->handshake_client([=](const auto& ec) {
						if (ec)
							return;

						std::ostringstream os;

						{
							detail::oarchive oa(os, c);

							std::function<void(const function_type&)> receiver = [=](const auto& f) {
								f(args...);
							};

							oa(receiver);
						}

						c->invoke_remote(0, os.str());
						c->receive();
					});
				});
			});
		}

	private:
		void run()
		{
			io_service_.run();
		}

		std::string host_;
		unsigned short port_;
		boost::asio::io_service io_service_;
		boost::asio::ip::tcp::resolver resolver_{io_service_};
		std::unique_ptr<boost::asio::io_service::work> work_ = std::make_unique<boost::asio::io_service::work>(io_service_);
		std::thread thread_;
	};
}
