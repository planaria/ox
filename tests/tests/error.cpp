#include <catch.hpp>
#include <future>
#include <ox/ox.hpp>

TEST_CASE("error")
{
	ox::client<void()> client("0.0.0.0");

	std::promise<boost::system::error_code> error;
	auto f = error.get_future();

	client([&](const auto& ec) {
		error.set_value(ec);
	});

	using namespace std::chrono_literals;
	REQUIRE(f.wait_for(1s) == std::future_status::ready);

	CHECK(f.get());
}
