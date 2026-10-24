#pragma once

#include "alias/alias.hpp"
#include "shapes/shape.hpp"
#include "shapes/shape_instance.hpp"

#include "tkit/math/quaternion.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/memory/ptr.hpp"
#include "tkit/utils/optional.hpp"

namespace Lim
{
class Shell;

/// A rigid kit shape that drifts and spins, colliding with other bodies and the shell by its true, evolving shape.
class Body
{
  public:
    struct Motion
    {
        f32v3 Position;
        f32q Orientation;
        f32v3 Velocity;
        f32v3 AngularVelocity;
    };

    struct Penetration
    {
        f32v3 Normal;
        f32 Depth;
    };

    Body(TKit::Scope<Shape> shape, const Motion &motion, f32 density);

    /// Drops the inverse mass, inertia and velocities, so the body stays still under every collision.
    void Anchor();
    /// Poses the shape at the absolute game time.
    void Evolve(f32 seconds);
    /// Moves and spins the body; angularDamping is the share of spin lost per second.
    void Advance(f32 deltaTime, f32 angularDamping);
    void Collide(Body &other, f32 restitution);
    void CollideWithShell(const Shell &shell, f32 restitution);

    /// The push that moves a sphere out of the body along the distance gradient, when they overlap.
    [[nodiscard]] TKit::Optional<Penetration> Penetrate(const f32v3 &center, f32 radius) const;
    [[nodiscard]] const f32v3 &GetPosition() const;
    [[nodiscard]] f32 GetBoundingRadius() const;
    [[nodiscard]] ShapeInstance GetInstance(u32 packedColor) const;

  private:
    struct Contact
    {
        void Add(const f32v3 &point, const f32v3 &normal, f32 depth);

        f32v3 WeightedPoint{0.f};
        f32v3 WeightedNormal{0.f};
        f32 Weight = 0.f;
        f32 Depth = 0.f;
    };

    void GatherPenetrations(const Body &target, Contact &contact) const;
    [[nodiscard]] f32v3 GetPointVelocity(const f32v3 &offset) const;
    [[nodiscard]] f32 GetImpulseResistance(const f32v3 &offset, const f32v3 &normal) const;
    void ApplyImpulse(const f32v3 &offset, const f32v3 &impulse);

    TKit::Scope<Shape> m_Shape;
    f32v3 m_Position;
    f32q m_Orientation;
    f32v3 m_Velocity;
    f32v3 m_AngularVelocity;
    f32 m_InverseMass = 0.f;
    f32 m_InverseInertia = 0.f;
};
} // namespace Lim
