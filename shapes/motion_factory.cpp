#include "shapes/motion_factory.hpp"
#include "shapes/piston_motion.hpp"
#include "shapes/ratchet_motion.hpp"
#include "shapes/slide_motion.hpp"
#include "shapes/still_motion.hpp"

#include "tkit/container/fixed_array.hpp"

namespace Lim
{
namespace
{
const StillMotion s_Still{};
const PistonMotion s_Piston{};
const RatchetMotion s_Ratchet{};
const SlideMotion s_Slide{};

const TKit::FixedArray<const MotionStrategy *, MOTION_KIND_COUNT> s_Strategies{&s_Still, &s_Piston, &s_Ratchet,
                                                                               &s_Slide};

static_assert(u32(MotionKind::Still) == 0 && u32(MotionKind::Piston) == 1 && u32(MotionKind::Ratchet) == 2 &&
                  u32(MotionKind::Slide) == 3,
              "s_Strategies lists one strategy per MotionKind, in order");
} // namespace

const MotionStrategy &MotionFactory::Get(const MotionKind kind)
{
    return *s_Strategies[u32(kind)];
}
} // namespace Lim
