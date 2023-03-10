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

struct interval {
  clock::time_point start{ clock::now() };

  clock::duration get() const noexcept
  {
    return clock::now() - start;
  }

  double s() const noexcept
  {
    return duration_cast<seconds>(get()).count();
  }

  double ms() const noexcept
  {
    return duration_cast<milliseconds>(get()).count();
  }

  double us() const noexcept
  {
    return duration_cast<microseconds>(get()).count();
  }

  double ns() const noexcept
  {
    return duration_cast<nanoseconds>(get()).count();
  }
};

}  // namespace epos