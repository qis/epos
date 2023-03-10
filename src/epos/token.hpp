#pragma once
#include <boost/asio/as_tuple.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace epos {

using token = decltype(boost::asio::as_tuple(boost::asio::use_awaitable));

}  // namespace epos