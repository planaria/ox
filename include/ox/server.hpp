#pragma once
#include "detail/archive.hpp"
#include "detail/connection.hpp"
#include <boost/asio.hpp>
#include <memory>
#include <thread>

namespace ox
{
	template <class Function>
	class server;

	template <class Result, class... Arguments>
	class server<Result(Arguments...)>
	{
	public:
		static_assert(std::is_void<Result>::value, "It is not allowed to use return value since it is difficult to gurantee returning");

		typedef std::function<void(Arguments...)> function_type;

		explicit server(function_type function, unsigned short port = 21872)
			: function_(function)
			, acceptor_(io_service_, local_endpoint(port))
		{
			accept();
			thread_ = std::thread(std::bind(&server::run, this));
		}

		~server()
		{
			io_service_.stop();
			thread_.join();
		}

	private:
		static boost::asio::ip::tcp::endpoint local_endpoint(unsigned short port)
		{
			return boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port);
		}

		void accept()
		{
			auto f = function_;
			auto c = std::make_shared<detail::connection>(io_service_, [](const auto& /*ec*/) {});

			c->resgister_callback([=](const auto& str) {
				std::istringstream is(str);
				detail::iarchive ia(is, c);

				std::function<void(const function_type&)> receiver;
				ia(receiver);

				receiver(f);
			});

			acceptor_.async_accept(c->socket(), [=](const auto& ec) {
				if (ec == boost::asio::error::operation_aborted)
					return;

				if (!ec)
				{
					c->handshake_server([=](const auto& ec) {
						if (ec)
							return;

						c->receive([](const auto& /*ec*/) {
						});
					});
				}

				this->accept();
			});
		}

		void run()
		{
			io_service_.run();
		}

		function_type function_;
		boost::asio::io_service io_service_;
		boost::asio::ip::tcp::acceptor acceptor_;
		std::thread thread_;
	};
}
