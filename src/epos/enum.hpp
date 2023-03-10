#pragma once
#include <boost/describe/enum.hpp>
#include <boost/describe/enumerators.hpp>
#include <boost/preprocessor/tuple.hpp>
#include <algorithm>
#include <array>
#include <cassert>

// clang-format off

#define EPOS_TUPLE_ELEM_0(e) BOOST_PP_TUPLE_ELEM_O_2(0, e)
#define EPOS_TUPLE_ELEM_1(e) BOOST_PP_TUPLE_ELEM_O_2(1, e)

#define EPOS_ENUM_DEFINITION(T, e) EPOS_TUPLE_ELEM_0(e) = EPOS_TUPLE_ELEM_1(e),
#define EPOS_ENUM_ENTRY(T, e) BOOST_DESCRIBE_ENUM_ENTRY(T, EPOS_TUPLE_ELEM_0(e))

#define EPOS_ENUM_DEFINE_CLASS(E, ...)                                    \
  enum class EPOS_TUPLE_ELEM_0(E) : EPOS_TUPLE_ELEM_1(E){                        \
    BOOST_DESCRIBE_PP_FOR_EACH(EPOS_ENUM_DEFINITION, E, __VA_ARGS__)             \
  };                                                                             \
  BOOST_DESCRIBE_ENUM_BEGIN(EPOS_TUPLE_ELEM_0(E))                                \
  BOOST_DESCRIBE_PP_FOR_EACH(EPOS_ENUM_ENTRY, EPOS_TUPLE_ELEM_0(E), __VA_ARGS__) \
  BOOST_DESCRIBE_ENUM_END(EPOS_TUPLE_ELEM_0(E))

#define EPOS_ENUM_DEFINE_NESTED_CLASS(E, ...)                                    \
  enum class EPOS_TUPLE_ELEM_0(E) : EPOS_TUPLE_ELEM_1(E){                        \
    BOOST_DESCRIBE_PP_FOR_EACH(EPOS_ENUM_DEFINITION, E, __VA_ARGS__)             \
  };                                                                             \
  friend BOOST_DESCRIBE_ENUM_BEGIN(EPOS_TUPLE_ELEM_0(E))                         \
  BOOST_DESCRIBE_PP_FOR_EACH(EPOS_ENUM_ENTRY, EPOS_TUPLE_ELEM_0(E), __VA_ARGS__) \
  BOOST_DESCRIBE_ENUM_END(EPOS_TUPLE_ELEM_0(E))

// clang-format on

namespace epos {
namespace detail {

template <class L>
inline constexpr std::size_t enum_size = 0;

template <template <class...> class L, class... T>
inline constexpr std::size_t enum_size<L<T...>> = sizeof...(T);

template <class E, template <class... T> class L, class... T>
inline constexpr std::array<E, sizeof...(T)> enum_as_array(L<T...>) noexcept
{
  return { T::value... };
}

}  // namespace detail

template <class E>
struct enum_size {
  static constexpr std::size_t value = detail::enum_size<boost::describe::describe_enumerators<E>>;
};

template <class E>
inline constexpr std::size_t enum_size_v = enum_size<E>::value;

template <class E>
inline constexpr auto enum_as_array() noexcept
{
  return detail::enum_as_array<E>(boost::describe::describe_enumerators<E>());
}

template <class E>
inline constexpr E enum_entry(std::size_t index) noexcept
{
  assert(index < enum_size_v<E>);
  return enum_as_array<E>()[index];
}

template <class E>
inline constexpr std::size_t enum_index(E entry) noexcept
{
  const auto ea = enum_as_array<E>();
  const auto it = std::find(ea.begin(), ea.end(), entry);
  assert(it != ea.end());
  return static_cast<std::size_t>(std::distance(ea.begin(), it));
}

}  // namespace epos