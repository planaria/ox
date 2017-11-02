#pragma once
#include <tuple>

namespace ox
{
	namespace detail
	{
		template <class Function, class Tuple, std::size_t... Indices>
		void apply_impl(Function f, Tuple&& t, std::index_sequence<Indices...>)
		{
			using dummy = int[];
			(void)dummy{0, (f(std::get<Indices>(t)...), 0)};
		}

		template <class Function, class Tuple>
		void apply(Function f, Tuple&& t)
		{
			constexpr auto size = std::tuple_size<std::remove_reference_t<Tuple>>::value;
			apply_impl(f, std::forward<Tuple>(t), std::make_index_sequence<size>());
		}
	}
}
