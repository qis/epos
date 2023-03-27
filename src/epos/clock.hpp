#pragma once
#include <chrono>

namespace epos {

using namespace std::chrono_literals;

using clock = std::chrono::high_resolution_clock;
using nanoseconds = std::chrono::duration<float, std::chrono::nanoseconds::period>;
using microseconds = std::chrono::duration<float, std::chrono::microseconds::period>;
using milliseconds = std::chrono::duration<float, std::chrono::milliseconds::period>;
using seconds = std::chrono::duration<float, std::chrono::seconds::period>;
using minutes = std::chrono::duration<float, std::chrono::minutes::period>;
using hours = std::chrono::duration<float, std::chrono::hours::period>;
using days = std::chrono::duration<float, std::chrono::days::period>;

using std::chrono::duration_cast;

struct interval {
  clock::time_point start{ clock::now() };

  clock::duration get() const noexcept
  {
    return clock::now() - start;
  }

  float s() const noexcept
  {
    return duration_cast<seconds>(get()).count();
  }

  float ms() const noexcept
  {
    return duration_cast<milliseconds>(get()).count();
  }

  float us() const noexcept
  {
    return duration_cast<microseconds>(get()).count();
  }

  float ns() const noexcept
  {
    return duration_cast<nanoseconds>(get()).count();
  }
};

}  // namespace epos