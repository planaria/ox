#include <catch.hpp>
#include <future>
#include <ox/ox.hpp>

TEST_CASE("simple")
{
	using function_type = void(int, std::function<void(const std::string&)>);

	ox::server<function_type> server([](auto x, auto f) {
		f(boost::lexical_cast<std::string>(x));
	});

	ox::client<function_type> client("localhost");

	std::promise<std::string> result;
	auto f = result.get_future();

	client(123, [&](auto str) {
		result.set_value(str);
	});

	CHECK(f.get() == "123");
}
