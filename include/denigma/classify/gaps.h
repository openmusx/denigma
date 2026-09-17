/*
 * Copyright (C) 2026, Robert Patterson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#pragma once

#include <optional>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "denigma/classify/chords.h"
#include "denigma/classify/expressions.h"
#include "denigma/classify/formatted_text.h"
#include "denigma/classify/noteheads.h"

namespace denigma {
namespace classify {

/// @struct GapPosition
/// @brief Position within an anchored measure as a whole-note fraction.
struct GapPosition
{
    int numerator{};
    int denominator{1};
};

/// @brief Converts a musx fraction of a whole note into a gap position.
inline GapPosition gapPositionFromFraction(const musx::util::Fraction& fraction)
{
    return {fraction.numerator(), fraction.denominator()};
}

/// @struct GapAnchor
/// @brief Stable target object identity and optional location within that object.
struct GapAnchor
{
    std::string id;
    std::optional<int> staff;
    std::optional<GapPosition> position;
};

/// @enum GapExtent
/// @brief How much of the feature the conversion target lacks.
enum class GapExtent {
    Complete, ///< Nothing in the target stands for the feature.
    Partial ///< The anchored object stands for the feature but lost the reported payload.
};

/// @struct GapPlacement
/// @brief One place the target document would draw a feature that is reported once.
///
/// A Finale staff list draws one marking on several staves. The gap for it is reported once, at
/// the object the marking belongs to, and lists where the target would draw it.
struct GapPlacement
{
    /// @enum Kind
    /// @brief What the placement's anchor names.
    enum class Kind {
        Staff, ///< A staff of a part measure: the anchor is the part measure with a staff.
        SystemTop, ///< The top staff of every system: the anchor is the global measure.
        SystemBottom ///< The bottom staff of every system: the anchor is the global measure.
    };

    Kind kind{Kind::Staff};
    GapAnchor anchor;
};

/// @struct PlaybackOnly
/// @brief Marker payload: the anchored object sounds in the source without being drawn, and the
/// target cannot hide it.
struct PlaybackOnly
{};

using GapPayload = std::variant<ChordSymbolClassification, NoteheadClassification, ExpressionClassification, FormattedText, PlaybackOnly>;

/// @struct Gap
/// @brief One classified feature omitted, in whole or in part, from a conversion target.
struct Gap
{
    GapAnchor anchor;
    GapExtent extent{GapExtent::Complete};
    /// @brief Where the target would draw the feature. Empty when #anchor alone says so.
    std::vector<GapPlacement> placements;
    GapPayload payload;
};

/// @class GapCollector
/// @brief Collects typed conversion gaps for inspection or later serialization.
///
/// Serialize or otherwise consume the collected gaps before releasing the parsed source document.
/// Some classification values may retain non-owning access to document-backed source objects.
class GapCollector
{
public:
    /// Adds one typed gap.
    template <typename Payload>
    void add(GapAnchor anchor, Payload payload, GapExtent extent = GapExtent::Complete, std::vector<GapPlacement> placements = {})
    {
        m_gaps.push_back({std::move(anchor), extent, std::move(placements), GapPayload(std::move(payload))});
    }

    /// Returns collected gaps in source traversal order.
    [[nodiscard]] std::span<const Gap> gaps() const noexcept { return m_gaps; }

private:
    std::vector<Gap> m_gaps;
};

} // namespace classify
} // namespace denigma
