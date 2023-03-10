#include "signature.hpp"
#include <algorithm>
#include <functional>

namespace epos {

std::size_t signature::find(
  const std::byte* begin,
  const std::byte* end,
  const std::byte* data,
  const std::byte* mask,
  std::size_t size) noexcept
{
  assert(begin && end);
  assert(begin < end);
  assert(data);
  assert(size);
  if (mask) {
    std::size_t mask_index = 0;
    const auto compare = [&](std::byte lhs, std::byte rhs) noexcept {
      if ((lhs & mask[mask_index++]) == rhs) {
        return true;
      }
      mask_index = 0;
      return false;
    };
    const auto searcher = std::default_searcher(data, data + size, compare);
    return static_cast<std::size_t>(std::search(begin, end, searcher) - begin);
  }
  const auto searcher = std::boyer_moore_horspool_searcher(data, data + size);
  return static_cast<std::size_t>(std::search(begin, end, searcher) - begin);
}

}  // namespace epos