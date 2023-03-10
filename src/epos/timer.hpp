#pragma once
#include <epos/token.hpp>
#include <boost/asio/steady_timer.hpp>

namespace epos {

using timer = decltype(token::as_default_on(boost::asio::steady_timer({})));

}  // namespace epos