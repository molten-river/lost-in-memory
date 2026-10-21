#include "lim/body.hpp"

#include "lim/shell.hpp"

#include "onyx/transform.hpp"
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

/// Runs a query that both shapes answer through the same member names.
template <typename Query> decltype(auto) QueryShape(const std::variant<Blob, SectionGraph> &shape, Query &&query)
{
    if (const Blob *const blob = std::get_if<Blob>(&shape))
        return query(*blob);
    return query(*std::get_if<SectionGraph>(&shape));
}
} // namespace

Body::Body(Blob shape, const Motion &motion, const f32 density)
    : m_Shape(std::move(shape)), m_Position(motion.Position), m_Orientation(motion.Orientation),
      m_Velocity(motion.Velocity), m_AngularVelocity(motion.AngularVelocity)
{
    ComputeMass(std::get_if<Blob>(&m_Shape)->GetVolume(), density);
}

Body::Body(const SectionGraph &shape, const Motion &motion, const f32 density)
    : m_Shape(shape), m_Position(motion.Position), m_Orientation(motion.Orientation), m_Velocity(motion.Velocity),
      m_AngularVelocity(motion.AngularVelocity)
{
    ComputeMass(shape.GetRestVolume(), density);
}

void Body::SetAnchored(const bool anchored)
{
    m_Anchored = anchored;
    m_InverseMass = anchored ? 0.f : m_FreeInverseMass;
    m_InverseInertia = anchored ? 0.f : m_FreeInverseInertia;
    if (!anchored)
        return;
    m_Velocity = f32v3{0.f};
    m_AngularVelocity = f32v3{0.f};
}

bool Body::IsAnchored() const
{
    return m_Anchored;
}

void Body::UpdateShape(const f32 time, const GraphParams &graph, const EvolutionParams &evolution, const LinkRule rule)
{
    if (SectionGraph *const sections = std::get_if<SectionGraph>(&m_Shape))
        sections->Update(time, graph, evolution, rule);
}

bool Body::Split(Pieces &pieces, PieceOffsets &offsets) const
{
    const SectionGraph *const sections = std::get_if<SectionGraph>(&m_Shape);
    if (!sections || sections->CountComponents() <= 1)
        return false;
    sections->Split(pieces, offsets);
    return true;
}

Body Body::MakePiece(const SectionGraph &piece, const f32v3 &offset, const f32 density) const
{
    const f32v3 worldOffset = m_Orientation * offset;
    return Body(piece,
                Motion{.Position = m_Position + worldOffset,
                       .Orientation = m_Orientation,
                       .Velocity = GetPointVelocity(worldOffset),
                       .AngularVelocity = m_AngularVelocity},
                density);
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
    if (m_InverseMass + other.m_InverseMass <= 0.f)
        return;

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
    if (m_Anchored || TKit::Math::Norm(m_Position) + GetBoundingRadius() < innerRadius)
        return;

    Contact contact{};
    const TKit::Span<const f32v3> samples =
        QueryShape(m_Shape, [](const auto &shape) { return shape.GetSamplePoints(); });
    for (const f32v3 &sample : samples)
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
    const f32 distance = QueryShape(m_Shape, [&local](const auto &shape) { return shape.Distance(local); });
    if (distance >= radius)
        return TKit::Optional<Penetration>::None();
    const f32v3 gradient = QueryShape(m_Shape, [&local](const auto &shape) { return shape.Gradient(local); });
    return Penetration{.Normal = m_Orientation * gradient, .Depth = radius - distance};
}

const f32v3 &Body::GetPosition() const
{
    return m_Position;
}

f32 Body::GetBoundingRadius() const
{
    return QueryShape(m_Shape, [](const auto &shape) { return shape.GetBoundingRadius(); });
}

void Body::Draw(Onyx::RenderContext<D3> &context) const
{
    if (const Blob *const blob = std::get_if<Blob>(&m_Shape))
        blob->Draw(context, ComputeTransform());
}

bool Body::AppendTriangles(const u32 packedColor, const TKit::Span<Onyx::DynamicVertex<D3>> out, u32 &written) const
{
    const SectionGraph *const sections = std::get_if<SectionGraph>(&m_Shape);
    return !sections || sections->AppendTriangles(ComputeTransform(), packedColor, out, written);
}

void Body::ComputeMass(const f32 volume, const f32 density)
{
    const f32 mass = TKit::Math::Max(volume, MIN_VOLUME) * density;
    const TKit::Span<const f32v3> samples =
        QueryShape(m_Shape, [](const auto &shape) { return shape.GetSamplePoints(); });

    const f32 boundingRadius = GetBoundingRadius();
    f32 squaredRadius = boundingRadius * boundingRadius;
    if (!samples.IsEmpty())
    {
        f32 sum = 0.f;
        for (const f32v3 &sample : samples)
            sum += TKit::Math::NormSquared(sample);
        squaredRadius = sum / f32(samples.GetSize());
    }

    m_FreeInverseMass = 1.f / mass;
    m_FreeInverseInertia = 1.f / (SOLID_SPHERE_INERTIA * mass * TKit::Math::Max(squaredRadius, MIN_SQUARED_RADIUS));
    m_InverseMass = m_FreeInverseMass;
    m_InverseInertia = m_FreeInverseInertia;
}

void Body::GatherPenetrations(const Body &target, Contact &contact) const
{
    const f32q toTarget = TKit::Math::Conjugate(target.m_Orientation);
    const f32q relative = toTarget * m_Orientation;
    const f32v3 origin = toTarget * (m_Position - target.m_Position);
    const f32 reach = target.GetBoundingRadius();

    const TKit::Span<const f32v3> samples =
        QueryShape(m_Shape, [](const auto &shape) { return shape.GetSamplePoints(); });
    for (const f32v3 &sample : samples)
    {
        const f32v3 local = relative * sample + origin;
        if (TKit::Math::NormSquared(local) >= reach * reach)
            continue;

        const f32 distance = QueryShape(target.m_Shape, [&local](const auto &shape) { return shape.Distance(local); });
        if (distance >= 0.f)
            continue;
        const f32v3 gradient =
            QueryShape(target.m_Shape, [&local](const auto &shape) { return shape.Gradient(local); });
        contact.Add(target.m_Position + target.m_Orientation * local, target.m_Orientation * gradient, -distance);
    }
}

void Body::Contact::Add(const f32v3 &point, const f32v3 &normal, const f32 depth)
{
    WeightedPoint += point * depth;
    WeightedNormal += normal * depth;
    Weight += depth;
    Depth = TKit::Math::Max(Depth, depth);
}

f32m4 Body::ComputeTransform() const
{
    return Onyx::Transform<D3>::ComputeTransform(m_Position, f32v3{1.f}, m_Orientation);
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
