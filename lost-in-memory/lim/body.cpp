#include "lim/body.hpp"

#include "lim/shell.hpp"

#include "onyx/transform.hpp"
#include "tkit/container/span.hpp"
#include "tkit/math/math.hpp"
#include <utility>

namespace Lim
{
using Onyx::D3;

namespace
{
constexpr f32 SOLID_SPHERE_INERTIA = 0.4f;
constexpr f32 MIN_VOLUME = 1e-4f;
constexpr f32 MIN_SQUARED_RADIUS = 1e-4f;
constexpr f32 MIN_NORMAL_LENGTH = 1e-6f;
} // namespace

Body::Body(Blob shape, const Motion &motion, const f32 density)
    : m_Shape(std::move(shape)), m_Position(motion.Position), m_Orientation(motion.Orientation),
      m_Velocity(motion.Velocity), m_AngularVelocity(motion.AngularVelocity)
{
    const f32 mass = TKit::Math::Max(m_Shape.GetVolume(), MIN_VOLUME) * density;
    const TKit::Span<const f32v3> samples = m_Shape.GetSamplePoints();

    f32 squaredRadius = m_Shape.GetBoundingRadius() * m_Shape.GetBoundingRadius();
    if (!samples.IsEmpty())
    {
        f32 sum = 0.f;
        for (const f32v3 &sample : samples)
            sum += TKit::Math::NormSquared(sample);
        squaredRadius = sum / f32(samples.GetSize());
    }

    m_InverseMass = 1.f / mass;
    m_InverseInertia = 1.f / (SOLID_SPHERE_INERTIA * mass * TKit::Math::Max(squaredRadius, MIN_SQUARED_RADIUS));
}

void Body::Advance(const f32 deltaTime, const f32 angularDamping)
{
    m_Position += m_Velocity * deltaTime;

    const f32 spin = TKit::Math::Norm(m_AngularVelocity);
    if (spin > 0.f)
        m_Orientation =
            TKit::Math::Normalize(f32q::FromAngleAxis(spin * deltaTime, m_AngularVelocity / spin) * m_Orientation);
    m_AngularVelocity *= TKit::Math::Power(1.f - angularDamping, deltaTime);
}

void Body::Collide(Body &other, const f32 restitution)
{
    const f32 reach = GetBoundingRadius() + other.GetBoundingRadius();
    if (TKit::Math::NormSquared(m_Position - other.m_Position) >= reach * reach)
        return;

    Contact pushedOut{};
    GatherPenetrations(other, pushedOut);
    Contact pushedIn{};
    other.GatherPenetrations(*this, pushedIn);

    const f32 weight = pushedOut.Weight + pushedIn.Weight;
    const f32v3 summedNormal = pushedOut.WeightedNormal - pushedIn.WeightedNormal;
    const f32 normalLength = TKit::Math::Norm(summedNormal);
    if (weight <= 0.f || normalLength <= MIN_NORMAL_LENGTH)
        return;

    const f32v3 normal = summedNormal / normalLength;
    const f32v3 point = (pushedOut.WeightedPoint + pushedIn.WeightedPoint) / weight;
    const f32v3 offset = point - m_Position;
    const f32v3 otherOffset = point - other.m_Position;

    const f32 approach = TKit::Math::Dot(GetPointVelocity(offset) - other.GetPointVelocity(otherOffset), normal);
    if (approach < 0.f)
    {
        const f32 impulse = -(1.f + restitution) * approach /
                            (GetImpulseResistance(offset, normal) + other.GetImpulseResistance(otherOffset, normal));
        ApplyImpulse(offset, normal * impulse);
        other.ApplyImpulse(otherOffset, normal * -impulse);
    }

    const f32 depth = TKit::Math::Max(pushedOut.Depth, pushedIn.Depth);
    const f32 share = m_InverseMass / (m_InverseMass + other.m_InverseMass);
    m_Position += normal * (depth * share);
    other.m_Position -= normal * (depth * (1.f - share));
}

void Body::CollideWithShell(const Shell &shell, const f32 restitution)
{
    const f32 innerRadius = shell.GetMinRadius();
    if (TKit::Math::Norm(m_Position) + GetBoundingRadius() < innerRadius)
        return;

    Contact contact{};
    for (const f32v3 &sample : m_Shape.GetSamplePoints())
    {
        const f32v3 point = m_Position + m_Orientation * sample;
        const f32 distanceSquared = TKit::Math::NormSquared(point);
        if (distanceSquared <= innerRadius * innerRadius)
            continue;

        const f32 distance = TKit::Math::SquareRoot(distanceSquared);
        const f32v3 outward = point / distance;
        const f32 depth = distance - shell.GetRadius(outward);
        if (depth > 0.f)
            contact.Add(point, -outward, depth);
    }

    const f32 normalLength = TKit::Math::Norm(contact.WeightedNormal);
    if (contact.Weight <= 0.f || normalLength <= MIN_NORMAL_LENGTH)
        return;

    const f32v3 normal = contact.WeightedNormal / normalLength;
    const f32v3 offset = contact.WeightedPoint / contact.Weight - m_Position;
    const f32 approach = TKit::Math::Dot(GetPointVelocity(offset), normal);
    if (approach < 0.f)
        ApplyImpulse(offset, normal * (-(1.f + restitution) * approach / GetImpulseResistance(offset, normal)));
    m_Position += normal * contact.Depth;
}

TKit::Optional<Body::Penetration> Body::Penetrate(const f32v3 &center, const f32 radius) const
{
    const f32v3 offset = center - m_Position;
    const f32 reach = GetBoundingRadius() + radius;
    if (TKit::Math::NormSquared(offset) >= reach * reach)
        return TKit::Optional<Penetration>::None();

    const f32v3 local = TKit::Math::Conjugate(m_Orientation) * offset;
    const f32 distance = m_Shape.Distance(local);
    if (distance >= radius)
        return TKit::Optional<Penetration>::None();
    return Penetration{.Normal = m_Orientation * m_Shape.Gradient(local), .Depth = radius - distance};
}

const f32v3 &Body::GetPosition() const
{
    return m_Position;
}

f32 Body::GetBoundingRadius() const
{
    return m_Shape.GetBoundingRadius();
}

void Body::Draw(Onyx::RenderContext<D3> &context) const
{
    m_Shape.Draw(context, Onyx::Transform<D3>::ComputeTransform(m_Position, f32v3{1.f}, m_Orientation));
}

void Body::GatherPenetrations(const Body &target, Contact &contact) const
{
    const f32q toTarget = TKit::Math::Conjugate(target.m_Orientation);
    const f32q relative = toTarget * m_Orientation;
    const f32v3 origin = toTarget * (m_Position - target.m_Position);
    const f32 reach = target.GetBoundingRadius();

    for (const f32v3 &sample : m_Shape.GetSamplePoints())
    {
        const f32v3 local = relative * sample + origin;
        if (TKit::Math::NormSquared(local) >= reach * reach)
            continue;

        const f32 distance = target.m_Shape.Distance(local);
        if (distance >= 0.f)
            continue;
        contact.Add(target.m_Position + target.m_Orientation * local,
                    target.m_Orientation * target.m_Shape.Gradient(local), -distance);
    }
}

void Body::Contact::Add(const f32v3 &point, const f32v3 &normal, const f32 depth)
{
    WeightedPoint += point * depth;
    WeightedNormal += normal * depth;
    Weight += depth;
    Depth = TKit::Math::Max(Depth, depth);
}

f32v3 Body::GetPointVelocity(const f32v3 &offset) const
{
    return m_Velocity + TKit::Math::Cross(m_AngularVelocity, offset);
}

f32 Body::GetImpulseResistance(const f32v3 &offset, const f32v3 &normal) const
{
    return m_InverseMass + TKit::Math::NormSquared(TKit::Math::Cross(offset, normal)) * m_InverseInertia;
}

void Body::ApplyImpulse(const f32v3 &offset, const f32v3 &impulse)
{
    m_Velocity += impulse * m_InverseMass;
    m_AngularVelocity += TKit::Math::Cross(offset, impulse) * m_InverseInertia;
}
} // namespace Lim
