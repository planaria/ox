#include <catch.hpp>
#include <future>
#include <ox/ox.hpp>

namespace
{
	struct test_service_if
	{
		std::function<void(std::function<void()>)> inc;
		std::function<void(std::function<void()>)> dec;
		std::function<void(std::function<void(int)>)> get;

		template <class Archive>
		void serialize(Archive& ar)
		{
			ar(inc, dec, get);
		}
	};

	class test_service
	{
	public:
		void inc()
		{
			++value_;
		}

		void dec()
		{
			--value_;
		}

		int get() const
		{
			return value_;
		}

	private:
		int value_ = 0;
	};
}

TEST_CASE("service")
{
	using function_type = void(const std::function<void(const test_service_if&)>&);

	ox::server<function_type> server([](auto f) {
		test_service_if s;
		auto impl = std::make_shared<test_service>();

		s.inc = [=](auto callback) {
			impl->inc();
			callback();
		};

		s.dec = [=](auto callback) {
			impl->dec();
			callback();
		};

		s.get = [=](auto callback) {
			callback(impl->get());
		};

		f(s);
	});

	ox::client<function_type> client("localhost");

	std::promise<int> result;
	auto f = result.get_future();

	client([&](auto s) {
		s.inc([&, s]() {
			s.inc([&, s]() {
				s.dec([&, s]() {
					s.get([&, s](auto x) {
						result.set_value(x);
					});
				});
			});
		});
	});

	using namespace std::chrono_literals;
	REQUIRE(f.wait_for(1s) == std::future_status::ready);

	CHECK(f.get() == 1);
}
