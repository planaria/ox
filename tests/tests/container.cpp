#include <catch.hpp>
#include <cereal/types/vector.hpp>
#include <future>
#include <numeric>
#include <ox/ox.hpp>

TEST_CASE("container")
{
	using function_type = void(const std::vector<int>&);

	std::promise<int> result;
	auto f = result.get_future();

	ox::server<function_type> server([&](const auto& v) {
		auto sum = std::accumulate(v.begin(), v.end(), 0);
		result.set_value(sum);
	});

	ox::client<function_type> client("localhost");

	client({1, 2, 3});

	using namespace std::chrono_literals;
	REQUIRE(f.wait_for(1s) == std::future_status::ready);

	CHECK(f.get() == 6);
}
