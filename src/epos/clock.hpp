#pragma once
#include <chrono>

namespace epos {

using namespace std::chrono_literals;

using clock = std::chrono::high_resolution_clock;
using nanoseconds = std::chrono::duration<double, std::chrono::nanoseconds::period>;
using microseconds = std::chrono::duration<double, std::chrono::microseconds::period>;
using milliseconds = std::chrono::duration<double, std::chrono::milliseconds::period>;
using seconds = std::chrono::duration<double, std::chrono::seconds::period>;
using minutes = std::chrono::duration<double, std::chrono::minutes::period>;
using hours = std::chrono::duration<double, std::chrono::hours::period>;
using days = std::chrono::duration<double, std::chrono::days::period>;

using std::chrono::duration_cast;

}  // namespace epos