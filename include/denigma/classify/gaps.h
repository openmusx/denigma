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
#include "denigma/classify/smartshapes.h"

namespace denigma {
namespace classify {

/// @struct GapAnchor
/// @brief Stable target object identity and optional location within that object.
struct GapAnchor
{
    std::string id;
    std::optional<int> staff;
    /// @brief Position within the anchored measure as a fraction of a whole note.
    std::optional<musx::util::Fraction> position;
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

using GapPayload =
    std::variant<ChordSymbolClassification, NoteheadClassification, ExpressionClassification, FormattedText, PlaybackOnly, SmartShapeClassification>;

/// @struct Gap
/// @brief One classified feature omitted, in whole or in part, from a conversion target.
struct Gap
{
    GapAnchor anchor;
    /// @brief Where a spanning feature ends. Empty for a feature that occupies one place.
    std::optional<GapAnchor> end;
    GapExtent extent{GapExtent::Complete};
    /// @brief Where the target would draw the feature. Empty when #anchor alone says so.
    std::vector<GapPlacement> placements;
    GapPayload payload;
};

/// @class GapCollector
/// @brief Collects typed conversion gaps for inspection or later serialization.
///
/// Classification values keep non-owning access to document-backed source objects, so the exporter
/// that records gaps hands the collector its source document with #retainDocument, and the collector
/// keeps the document alive until the collector itself is destroyed.
class GapCollector
{
public:
    /// Keeps @p document alive for the collector's lifetime.
    void retainDocument(musx::dom::DocumentPtr document) { m_documents.push_back(std::move(document)); }

    /// Adds one typed gap.
    template <typename Payload>
    void add(GapAnchor anchor, Payload payload, GapExtent extent = GapExtent::Complete, std::vector<GapPlacement> placements = {})
    {
        m_gaps.push_back({std::move(anchor), std::nullopt, extent, std::move(placements), GapPayload(std::move(payload))});
    }

    /// Adds one typed gap for a feature that spans from @p anchor to @p end.
    template <typename Payload>
    void addSpan(GapAnchor anchor, GapAnchor end, Payload payload, GapExtent extent = GapExtent::Complete)
    {
        m_gaps.push_back({std::move(anchor), std::move(end), extent, {}, GapPayload(std::move(payload))});
    }

    /// Returns collected gaps in source traversal order.
    [[nodiscard]] std::span<const Gap> gaps() const noexcept { return m_gaps; }

private:
    std::vector<Gap> m_gaps;
    std::vector<musx::dom::DocumentPtr> m_documents;
};

} // namespace classify
} // namespace denigma
