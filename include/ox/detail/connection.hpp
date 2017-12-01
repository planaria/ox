#pragma once
#include <boost/asio.hpp>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace ox
{
	namespace detail
	{
		class connection
			: public std::enable_shared_from_this<connection>
			, boost::noncopyable
		{
		public:
			explicit connection(
				boost::asio::io_service& io_service,
				const std::function<void(const boost::system::error_code&)>& error_handler)
				: socket_(io_service)
				, strand_(io_service)
				, error_handler_(error_handler)
			{
			}

			boost::asio::ip::tcp::socket& socket()
			{
				return socket_;
			}

			template <class Callback>
			void handshake_server(Callback callback)
			{
				auto self = this->shared_from_this();

				receive_signature([self, callback](const auto& ec) {
					if (ec)
					{
						callback(ec);
						return;
					}

					self->send_signature([self, callback](const auto& ec) {
						callback(ec);
					});
				});
			}

			template <class Callback>
			void handshake_client(Callback callback)
			{
				auto self = this->shared_from_this();

				send_signature([self, callback](const auto& ec) {
					if (ec)
					{
						callback(ec);
						return;
					}

					self->receive_signature([self, callback](const auto& ec) {
						callback(ec);
					});
				});
			}

			template <class Callback>
			void receive(Callback callback)
			{
				auto self = this->shared_from_this();

				receive_integer([self, callback](const auto& ec, std::uint64_t id) {
					if (ec)
					{
						callback(ec);
						return;
					}

					self->receive_integer([self, id, callback](const auto& ec, std::uint64_t size) {
						if (ec)
						{
							callback(ec);
							return;
						}

						if (size == std::numeric_limits<std::uint64_t>::max())
						{
							std::lock_guard<std::mutex> lock(self->mutex_);

							self->callback_map_.erase(id);
							self->receive(callback);
						}
						else
						{
							self->strand_.post([self, id, size, callback]() {
								auto str_buffer = std::make_shared<std::vector<char>>(size);

								boost::asio::async_read(self->socket_, boost::asio::buffer(*str_buffer), [self, id, str_buffer, callback](const auto& ec, auto /*bytes_transferred*/) {
									if (ec)
									{
										callback(ec);
										return;
									}

									std::function<void(const std::string&)> f;

									{
										std::lock_guard<std::mutex> lock(self->mutex_);

										auto it = self->callback_map_.find(id);
										if (it != self->callback_map_.end())
											f = it->second;
									}

									if (f)
										f(std::string(str_buffer->data(), str_buffer->size()));

									self->receive(callback);
								});
							});
						}
					});
				});
			}

			std::uint64_t resgister_callback(const std::function<void(const std::string&)>& callback)
			{
				std::lock_guard<std::mutex> lock(mutex_);

				auto index = index_++;
				callback_map_.insert(std::make_pair(index, callback));

				return index;
			}

			void unregister_callback_remote(std::uint64_t id)
			{
				auto self = this->shared_from_this();

				auto buffer = std::make_shared<std::vector<char>>();

				write_integer(*buffer, id);
				write_integer(*buffer, std::numeric_limits<std::uint64_t>::max());

				strand_.post([self, buffer]() {
					boost::asio::async_write(self->socket_, boost::asio::buffer(*buffer), [self, buffer](const auto& ec, auto /*bytes_transferred*/) {
						if (ec)
						{
							self->error_handler_(ec);
							return;
						}
					});
				});
			}

			void invoke_remote(std::uint64_t id, const std::string& str)
			{
				auto self = this->shared_from_this();

				auto buffer = std::make_shared<std::vector<char>>();

				write_integer(*buffer, id);
				write_integer(*buffer, str.size());

				buffer->insert(buffer->end(), str.begin(), str.end());

				strand_.post([self, buffer]() {
					boost::asio::async_write(self->socket_, boost::asio::buffer(*buffer), [self, buffer](const auto& ec, auto /*bytes_transferred*/) {
						if (ec)
						{
							self->error_handler_(ec);
							return;
						}
					});
				});
			}

		private:
			static const std::size_t signature_size = 3;

			constexpr static std::array<char, signature_size> get_signature()
			{
				return {{0x6f, 0x78, 0x00}};
			}

			template <class Callback>
			void send_signature(Callback callback)
			{
				auto self = this->shared_from_this();

				boost::asio::async_write(socket_, boost::asio::buffer(get_signature()), [self, callback](const auto& ec, auto /*bytes_transferred*/) {
					callback(ec);
				});
			}

			template <class Callback>
			void receive_signature(Callback callback)
			{
				auto self = this->shared_from_this();
				auto signature = std::make_shared<std::array<char, signature_size>>();

				boost::asio::async_read(socket_, boost::asio::buffer(*signature), [self, signature, callback](const auto& ec, auto /*bytes_transferred*/) {
					if (ec)
					{
						callback(ec);
						return;
					}

					if (*signature != get_signature())
					{
						callback(boost::system::errc::make_error_code(boost::system::errc::bad_message));
						return;
					}

					callback(ec);
				});
			}

			void write_integer(std::vector<char>& buffer, std::uint64_t value)
			{
				auto self = this->shared_from_this();

				if (value < 0x80)
				{
					buffer.push_back(static_cast<char>(value));
				}
				else if (value < 0x100)
				{
					buffer.push_back(static_cast<char>(0xcc));
					buffer.push_back(static_cast<char>(value));
				}
				else if (value < 0x10000)
				{
					buffer.push_back(static_cast<char>(0xcd));
					buffer.push_back(static_cast<char>((value >> 8) & 0xff));
					buffer.push_back(static_cast<char>(value & 0xff));
				}
				else if (value < 0x100000000)
				{
					buffer.push_back(static_cast<char>(0xce));
					buffer.push_back(static_cast<char>((value >> 24) & 0xff));
					buffer.push_back(static_cast<char>((value >> 16) & 0xff));
					buffer.push_back(static_cast<char>((value >> 8) & 0xff));
					buffer.push_back(static_cast<char>(value & 0xff));
				}
				else
				{
					buffer.push_back(static_cast<char>(0xcf));
					buffer.push_back(static_cast<char>((value >> 56) & 0xff));
					buffer.push_back(static_cast<char>((value >> 48) & 0xff));
					buffer.push_back(static_cast<char>((value >> 40) & 0xff));
					buffer.push_back(static_cast<char>((value >> 32) & 0xff));
					buffer.push_back(static_cast<char>((value >> 24) & 0xff));
					buffer.push_back(static_cast<char>((value >> 16) & 0xff));
					buffer.push_back(static_cast<char>((value >> 8) & 0xff));
					buffer.push_back(static_cast<char>(value & 0xff));
				}
			}

			template <class Callback>
			void receive_integer(Callback callback)
			{
				auto self = this->shared_from_this();

				strand_.post([self, callback]() {
					auto buffer = std::make_shared<std::vector<char>>(8);

					boost::asio::async_read(self->socket_, boost::asio::buffer(buffer->data(), 1), [self, buffer, callback](const auto& ec, auto /*bytes_transferred*/) {
						if (ec)
						{
							callback(ec, 0);
							return;
						}

						auto type = static_cast<std::uint8_t>((*buffer)[0]);

						if (type < 0x80)
						{
							callback(ec, type);
						}
						else
						{
							self->strand_.post([self, buffer, type, callback]() {
								std::size_t size;

								switch (type)
								{
								case 0xcc:
									size = 1;
									break;
								case 0xcd:
									size = 2;
									break;
								case 0xce:
									size = 4;
									break;
								case 0xcf:
									size = 8;
									break;
								default:
									callback(boost::system::errc::make_error_code(boost::system::errc::bad_message), 0);
									return;
								}

								boost::asio::async_read(self->socket_, boost::asio::buffer(buffer->data(), size), [self, buffer, size, callback](const auto& ec, auto /*bytes_transferred*/) {
									if (ec)
									{
										callback(ec, 0);
										return;
									}

									std::uint64_t value;
									auto p = buffer->data();

									switch (size)
									{
									case 1:
										value = static_cast<std::uint64_t>(p[0]);
										break;
									case 2:
										value = static_cast<std::uint64_t>(p[0]) << 8;
										value |= static_cast<std::uint64_t>(p[1]);
										break;
									case 4:
										value = static_cast<std::uint64_t>(p[0]) << 24;
										value |= static_cast<std::uint64_t>(p[1]) << 16;
										value |= static_cast<std::uint64_t>(p[2]) << 8;
										value |= static_cast<std::uint64_t>(p[3]);
										break;
									case 8:
										value = static_cast<std::uint64_t>(p[0]) << 56;
										value |= static_cast<std::uint64_t>(p[1]) << 48;
										value |= static_cast<std::uint64_t>(p[2]) << 40;
										value |= static_cast<std::uint64_t>(p[3]) << 32;
										value |= static_cast<std::uint64_t>(p[4]) << 24;
										value |= static_cast<std::uint64_t>(p[5]) << 16;
										value |= static_cast<std::uint64_t>(p[6]) << 8;
										value |= static_cast<std::uint64_t>(p[7]);
										break;
									default:
										callback(boost::system::errc::make_error_code(boost::system::errc::bad_message), 0);
										return;
									}

									callback(ec, value);
								});
							});
						}
					});
				});
			}

			boost::asio::ip::tcp::socket socket_;
			boost::asio::io_service::strand strand_;
			std::function<void(const boost::system::error_code&)> error_handler_;

			std::mutex mutex_;
			std::unordered_map<std::uint64_t, std::function<void(const std::string&)>> callback_map_;
			std::uint64_t index_ = 0;
		};
	}
}
