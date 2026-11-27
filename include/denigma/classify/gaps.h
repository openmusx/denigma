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

#include <cstddef>
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
#include "denigma/classify/text_blocks.h"

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

/// @struct PlaybackOnly
/// @brief Marker payload: the anchored object sounds in the source without being drawn, and the
/// target cannot hide it.
struct PlaybackOnly
{};

/// @brief A half-open range in one output text element. Units are Unicode scalar values for
/// text elements and glyph-list entries for glyph elements, measured before consumer edits.
struct GapTextRange
{
    std::string id;
    size_t offset{};
    size_t length{};
};

/// @brief Source formatting and characters for restoring a range's original appearance.
struct TextFormatting
{
    musx::dom::ResolvedFontInfo font;
    musx::dom::Evpu baseline{};
    musx::dom::Evpu superscript{};
    int tracking{};
    std::string sourceText;
    /// @brief The output text the formatting applies to.
    std::vector<GapTextRange> ranges;
};

/// @brief One emitted parser substitution, whose ranges may be replaced together.
struct TextInsert
{
    text::Insert insert;
    std::string sourceText;
    /// @brief The output text the substitution produced, in order.
    std::vector<GapTextRange> ranges;
};

// Each payload below pairs a target-neutral classification with what the target made of it: the
// target ids the exporter wrote, which a classification never holds. See design-decisions.md.

/// @struct SmartShapeGap
/// @brief A smart shape and, for one that spans, the target place it ends.
struct SmartShapeGap
{
    SmartShapeClassification classification;
    /// @brief Where the shape ends. Empty for a shape that occupies one place.
    std::optional<GapAnchor> end;
};

/// @struct ExpressionGap
/// @brief An expression and the target places its staff-list members would draw it.
struct ExpressionGap
{
    /// @struct Placement
    /// @brief One member of the classification drawn at one target place.
    struct Placement
    {
        /// @brief Index into ExpressionClassification::members of #classification.
        size_t member{};
        /// @brief The target place: the global measure for a floating member, the part measure and staff otherwise.
        GapAnchor anchor;
    };

    ExpressionClassification classification;
    /// @brief The members this gap lists, each at its target place. Empty when the anchor alone says where.
    std::vector<Placement> placements;
};

/// @struct PageTextGap
/// @brief Page text and the target pages it is drawn on.
struct PageTextGap
{
    /// @struct Page
    /// @brief One page the target wrote that shows the text.
    struct Page
    {
        /// @brief The target page's id.
        std::string pageId;
        /// @brief Index into PageTextClassification::placements of #classification, whose position applies.
        size_t placement{};
    };

    /// @brief The page text as classified, with every placement in the source.
    PageTextClassification classification;
    /// @brief The placements the target wrote a page for. Pages it did not write are absent.
    std::vector<Page> pages;
};

using GapPayload = std::variant<ChordSymbolClassification, NoteheadClassification, ExpressionGap, FormattedText, PlaybackOnly, SmartShapeGap,
    TextFormatting, TextInsert, MeasureTextClassification, PageTextGap>;

/// @struct Gap
/// @brief One classified feature omitted, in whole or in part, from a conversion target.
///
/// What only some payloads need, such as where a span ends or which output text a formatting gap
/// covers, belongs to the payload, not to the gap.
struct Gap
{
    /// @brief The target object the gap belongs to. Empty for a feature that belongs to no target
    /// object and is located by its payload, such as page text.
    std::optional<GapAnchor> anchor;
    GapExtent extent{GapExtent::Complete};
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
    void add(GapAnchor anchor, Payload payload, GapExtent extent = GapExtent::Complete)
    {
        m_gaps.push_back({std::move(anchor), extent, GapPayload(std::move(payload))});
    }

    /// Adds one typed gap that belongs to no target object, located by its payload alone, as page text
    /// is by its pages.
    template <typename Payload>
    void addUnanchored(Payload payload, GapExtent extent = GapExtent::Complete)
    {
        m_gaps.push_back({std::nullopt, extent, GapPayload(std::move(payload))});
    }

    /// Returns collected gaps in source traversal order.
    [[nodiscard]] std::span<const Gap> gaps() const noexcept { return m_gaps; }

private:
    std::vector<Gap> m_gaps;
    std::vector<musx::dom::DocumentPtr> m_documents;
};

} // namespace classify
} // namespace denigma
