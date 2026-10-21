#pragma once

#include "lim/alias.hpp"
#include "lim/blob.hpp"
#include "lim/form_recipe.hpp"
#include "lim/formula_params.hpp"
#include "lim/section_graph.hpp"

#include "onyx/context.hpp"
#include "onyx/vertex.hpp"
#include "tkit/container/span.hpp"
#include "tkit/container/static_array.hpp"
#include "tkit/math/quaternion.hpp"
#include "tkit/math/tensor.hpp"
#include "tkit/utils/optional.hpp"
#include <variant>

namespace Lim
{
class Shell;

/// A rigid blob or section graph that drifts and spins, colliding with other bodies and the shell by its true shape.
class Body
{
  public:
    using Pieces = TKit::StaticArray<SectionGraph, SectionGraph::MAX_SECTIONS>;
    using PieceOffsets = TKit::StaticArray<f32v3, SectionGraph::MAX_SECTIONS>;

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

    Body(Blob shape, const Motion &motion, f32 density);
    Body(const SectionGraph &shape, const Motion &motion, f32 density);

    /// An anchored body has no inverse mass or inertia and stays still until it is released.
    void SetAnchored(bool anchored);
    [[nodiscard]] bool IsAnchored() const;
    void UpdateShape(f32 time, const GraphParams &graph, const EvolutionParams &evolution, LinkRule rule);

    /// Fills pieces and their body-local offsets when a section graph has come apart; false for a whole shape.
    [[nodiscard]] bool Split(Pieces &pieces, PieceOffsets &offsets) const;
    /// A free body for one piece of this one, carrying this body's orientation, spin and point velocity there.
    [[nodiscard]] Body MakePiece(const SectionGraph &piece, const f32v3 &offset, f32 density) const;

    /// Moves and spins the body; angularDamping is the share of spin lost per second.
    void Advance(f32 deltaTime, f32 angularDamping);
    void Collide(Body &other, f32 restitution);
    void CollideWithShell(const Shell &shell, f32 restitution);

    /// The push that moves a sphere out of the body along the field gradient, when they overlap.
    [[nodiscard]] TKit::Optional<Penetration> Penetrate(const f32v3 &center, f32 radius) const;
    [[nodiscard]] const f32v3 &GetPosition() const;
    [[nodiscard]] f32 GetBoundingRadius() const;
    /// Draws a blob's static mesh; a section graph is drawn through AppendTriangles instead.
    void Draw(Onyx::RenderContext<Onyx::D3> &context) const;
    /// Appends a section graph's world-space triangles; a blob appends nothing and succeeds.
    [[nodiscard]] bool AppendTriangles(u32 packedColor, TKit::Span<Onyx::DynamicVertex<Onyx::D3>> out,
                                       u32 &written) const;

  private:
    struct Contact
    {
        void Add(const f32v3 &point, const f32v3 &normal, f32 depth);

        f32v3 WeightedPoint{0.f};
        f32v3 WeightedNormal{0.f};
        f32 Weight = 0.f;
        f32 Depth = 0.f;
    };

    void ComputeMass(f32 volume, f32 density);
    void GatherPenetrations(const Body &target, Contact &contact) const;
    [[nodiscard]] f32m4 ComputeTransform() const;
    [[nodiscard]] f32v3 GetPointVelocity(const f32v3 &offset) const;
    [[nodiscard]] f32 GetImpulseResistance(const f32v3 &offset, const f32v3 &normal) const;
    void ApplyImpulse(const f32v3 &offset, const f32v3 &impulse);

    std::variant<Blob, SectionGraph> m_Shape;
    f32v3 m_Position;
    f32q m_Orientation;
    f32v3 m_Velocity;
    f32v3 m_AngularVelocity;
    f32 m_InverseMass = 0.f;
    f32 m_InverseInertia = 0.f;
    f32 m_FreeInverseMass = 0.f;
    f32 m_FreeInverseInertia = 0.f;
    bool m_Anchored = false;
};
} // namespace Lim
