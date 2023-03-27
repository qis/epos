#include "view.hpp"

using namespace DirectX;
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/stats.hpp>

namespace epos::overwatch {

XMVECTOR view::predict(std::size_t entity, clock::time_point tp, milliseconds duration) noexcept
{
  using stats = boost::accumulators::stats<boost::accumulators::tag::mean>;
  using accumulator_set = boost::accumulators::accumulator_set<XMVECTOR, stats>;

  // Movement vector multiplier.
  static const auto mm = XMVectorSet(1.0f, 0.6f, 1.0f, 0.0f);

  // Get movement entry and element count.
  const auto& e = movement_[entity];
  const auto size = e.size();
  if (size < 2) {
    return {};
  }

  // Last position time point.
  const auto t1 = e[0].tp;

  // First position time point.
  auto t0 = e[1].tp;

  // Last known position.
  auto p0 = e[1].p0;

  // Movement vector count.
  auto mc = 1.0f;

  accumulator_set acc;
  acc(e[0].p0 - e[1].p0);

  for (std::size_t i = 2; i < size; i++) {
    // Get movement vector.
    acc(p0 - e[i].p0);

    // Update last known position.
    p0 = e[i].p0;

    // Update first position time point.
    t0 = e[i].tp;

    // Update movement vector count.
    mc += 1.0f;
  }

  // Mean movement vector.
  const auto mv = boost::accumulators::mean(acc);

  // Mean movement vector duration.
  const auto md = std::chrono::duration_cast<epos::milliseconds>(t1 - t0).count() / mc;

  // Mean movement vector prediction duration.
  const auto mp = std::chrono::duration_cast<epos::milliseconds>(tp - t1 + duration).count();

  // Predict movement.
  return mp / md * mm * mv;
}

}  // namespace epos::overwatch