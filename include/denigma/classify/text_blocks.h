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
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
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
#include <vector>

#include "denigma/classify/formatted_text.h"
#include "musx/musx.h"

namespace denigma {
namespace classify {

/// @struct TextBlockFrame
/// @brief The standard frame of a Finale text block: the rectangle its text is laid out in and the border drawn around it.
///
/// Every length is in EVPU at 100% scaling (288 per inch, 24 per staff space), fractional where
/// Finale stores the value in Efix. A custom frame shape is flagged but not described.
struct TextBlockFrame
{
    /// @brief Width of the frame, or zero when the frame expands to fit the text.
    musx::dom::Evpu width{};
    /// @brief Height of the frame, or zero when the frame expands to fit the text.
    musx::dom::Evpu height{};
    /// @brief Horizontal offset of the frame from the assignment's handle, positive to the right.
    musx::dom::Evpu handleOffsetX{};
    /// @brief Vertical offset of the frame from the assignment's handle, positive upward.
    musx::dom::Evpu handleOffsetY{};
    /// @brief Whether lines wrap at the frame's width. Without wrapping, only hard line breaks start a line.
    bool wordWrap{};
    /// @brief Whether full justification leaves a line holding a single word unstretched.
    bool noExpandSingleWord{};
    /// @brief How lines are laid out between the frame's edges.
    musx::dom::others::TextBlock::TextJustify justify{};
    /// @brief Distance between the frame and the text on every side.
    musx::dom::EvpuFloat inset{};
    /// @brief Line spacing as a percent of the font's natural spacing, when Finale sets it automatically.
    std::optional<int> lineSpacingPercent;
    /// @brief Line spacing as a fixed distance, when Finale sets it to a value.
    std::optional<musx::dom::Evpu> lineSpacing;
    /// @brief Whether the frame's border is drawn.
    bool showBorder{};
    /// @brief Thickness of the border.
    musx::dom::EvpuFloat borderThickness{};
    /// @brief Radius of the frame's corners, or zero for square corners.
    musx::dom::EvpuFloat cornerRadius{};
    /// @brief Whether Finale positions the first baseline from the frame's top edge, as text blocks created since Finale 3.6 do.
    bool positionFromFrameEdge{};
    /// @brief Whether the text block uses a Shape Designer shape instead of the standard frame.
    /// The shape itself is not described; the other fields still hold the standard frame's values.
    bool hasCustomShape{};
};

/// @brief Classifies the standard frame of a Finale text block.
TextBlockFrame classifyTextBlockFrame(const musx::dom::others::TextBlock& textBlock);

/// @struct MeasureTextClassification
/// @brief A Finale text block attached to a staff in a measure.
///
/// The assignment's handle is at #position in the measure, displaced by the two offsets. The
/// frame's own offsets are relative to the handle.
struct MeasureTextClassification
{
    /// @brief The assignment, which states its staff, measure and inci.
    musx::dom::MusxInstance<musx::dom::details::MeasureTextAssign> assignment;
    /// @brief The text, empty when the text block has none.
    FormattedText text;
    /// @brief The frame the text is laid out in.
    TextBlockFrame frame;
    /// @brief Metric position of the handle in the measure, as a fraction of a whole note.
    musx::util::Fraction position;
    /// @brief Fixed horizontal displacement of the handle from the start of the measure, kept in any
    /// spacing. Finale uses it in place of #position, so it is nonzero only when #position is zero.
    musx::dom::Evpu horizontalOffset{};
    /// @brief Vertical displacement of the handle from the staff's top line, positive upward.
    musx::dom::Evpu verticalOffset{};
    /// @brief Above when the frame, at the handle plus its own offset, lies above the staff's top line,
    /// below when it lies below the bottom line, and `NotApplicable` when it lies within the staff.
    musx::dom::VerticalPlacement placement{musx::dom::VerticalPlacement::NotApplicable};
};

/// @brief Classifies the text blocks attached to staff @p staffId in @p measure, as the part the measure was
/// requested for has them.
///
/// Each assignment that is shown, has a text block, and whose staff exists at the measure is one classification,
/// in assignment order.
std::vector<MeasureTextClassification> classifyMeasureTexts(
    const musx::dom::MusxInstance<musx::dom::others::Measure>& measure, musx::dom::StaffCmper staffId);

/// @struct PageTextPlacement
/// @brief One page of one score or linked part that shows a page text.
struct PageTextPlacement
{
    /// @brief The linked part (or the score) whose page this is.
    musx::dom::Cmper partId{};
    /// @brief The part's own page number, counting from 1 including its leading blank pages.
    musx::dom::PageCmper page{};
    /// @brief The part's own assignment: where the part puts the text, and the context to render it for this page.
    musx::dom::MusxInstance<musx::dom::others::PageTextAssign> assignment;
};

/// @struct PageTextClassification
/// @brief A Finale text block attached to one page or a range of pages, with every page of every
/// classified score or linked part that shows it.
///
/// A linked part shares its page text with the score and can unlink only its offsets, whether right-hand
/// pages are positioned independently with their own offsets, and its visibility. The text and frame
/// are therefore classified once, from the full score's assignment, whether or not the full score shows
/// the text, and #placements say where each part draws it. The assignment's own fields (its page
/// assignment ids, page parity, alignment and offsets) are read from #assignment, not copied.
struct PageTextClassification
{
    /// @brief The full score's assignment, which states everything a part cannot unlink.
    musx::dom::MusxInstance<musx::dom::others::PageTextAssign> assignment;
    /// @brief The text as a template: an insert that varies by page, such as the page number, is a run
    /// recording the insert, so it can be resolved again for each page.
    FormattedText text;
    /// @brief The frame the text is laid out in.
    TextBlockFrame frame;
    /// @brief Every page that shows the text, part by part in the order the parts were classified and
    /// page by page within each part. A part that hides the text has none.
    std::vector<PageTextPlacement> placements;
};

/// @brief Classifies the page-attached text blocks of @p document as the parts @p partIds show them.
///
/// Each assignment that some listed part shows and that has a text block is one classification, in
/// order of first appearance. A part's placements cover the pages in its page list, whether or not
/// their layout was calculated; an exporter that wrote fewer pages drops the rest.
std::vector<PageTextClassification> classifyPageTexts(const musx::dom::DocumentPtr& document, const std::vector<musx::dom::Cmper>& partIds);

} // namespace classify
} // namespace denigma
