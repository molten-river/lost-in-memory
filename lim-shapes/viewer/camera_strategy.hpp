#pragma once

#include "alias/alias.hpp"
#include "viewer/viewer_params.hpp"

#include "onyx/camera.hpp"
#include "onyx/transform.hpp"
#include "onyx/window.hpp"

#include "tkit/container/span.hpp"
#include "tkit/math/tensor.hpp"

namespace Lim
{
/// What the camera looks after: a point to centre on and how far the shape there reaches from it.
struct CameraFocus
{
    f32v3 Target;
    f32 Radius;
};

struct CameraFrame
{
    const Onyx::Window &Window;
    const CameraParams &Params;
    CameraFocus Focus;
    f32 DeltaTime;
    bool AcceptsInput;
};

/// One way of steering the view; switching strategies takes over the current pose instead of jumping.
class CameraStrategy
{
  public:
    virtual ~CameraStrategy() = default;

    virtual void Engage(const Onyx::Transform<Onyx::D3> &view, const CameraFrame &frame) = 0;
    virtual void Update(const CameraFrame &frame) = 0;
    virtual void ApplyTo(Onyx::Camera<Onyx::D3> &camera) const = 0;
    [[nodiscard]] virtual TKit::StringView GetName() const = 0;
};

[[nodiscard]] inline f32 KeyAxis(const Onyx::Window &window, const Onyx::Key positive, const Onyx::Key negative)
{
    return f32(window.IsKeyPressed(positive)) - f32(window.IsKeyPressed(negative));
}
} // namespace Lim
