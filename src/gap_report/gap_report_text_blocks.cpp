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

#include <string_view>

#include "gap_report_json.h"

namespace denigma {
namespace gap_report {

namespace {

using namespace musx::dom;

std::string_view justifyName(others::TextBlock::TextJustify value)
{
    using Enum = others::TextBlock::TextJustify;
    switch (value) {
    case Enum::Left: return "left";
    case Enum::Center: return "center";
    case Enum::Right: return "right";
    case Enum::Full: return "full";
    case Enum::ForcedFull: return "forced-full";
    }
    return "unknown";
}

std::string_view horizontalAlignmentName(AlignJustify value)
{
    switch (value) {
    case AlignJustify::Left: return "left";
    case AlignJustify::Center: return "center";
    case AlignJustify::Right: return "right";
    }
    return "unknown";
}

std::string_view verticalAlignmentName(others::PageTextAssign::VerticalAlignment value)
{
    using Enum = others::PageTextAssign::VerticalAlignment;
    switch (value) {
    case Enum::Top: return "top";
    case Enum::Center: return "center";
    case Enum::Bottom: return "bottom";
    }
    return "unknown";
}

std::string_view pageAssignTypeName(others::PageTextAssign::PageAssignType value)
{
    using Enum = others::PageTextAssign::PageAssignType;
    switch (value) {
    case Enum::AllPages: return "all";
    case Enum::Even: return "even";
    case Enum::Odd: return "odd";
    }
    return "unknown";
}

} // namespace

json pageTextPositionJson(const musx::dom::others::PageTextAssign& assignment)
{
    json result{
        {"offsetX", assignment.xDisp},
        {"offsetY", assignment.yDisp},
        {"independentRightPages", assignment.indRpPos},
    };
    if (assignment.indRpPos) {
        result["rightPageOffsetX"] = assignment.rightPgXDisp;
        result["rightPageOffsetY"] = assignment.rightPgYDisp;
    }
    return result;
}

json textBlockFrameJson(const classify::TextBlockFrame& frame)
{
    json result{
        {"unit", "evpu"},
        {"width", frame.width},
        {"height", frame.height},
        {"handleOffsetX", frame.handleOffsetX},
        {"handleOffsetY", frame.handleOffsetY},
        {"wordWrap", frame.wordWrap},
        {"noExpandSingleWord", frame.noExpandSingleWord},
        {"justify", justifyName(frame.justify)},
        {"inset", frame.inset},
    };
    if (frame.lineSpacingPercent) {
        result["lineSpacingPercent"] = *frame.lineSpacingPercent;
    }
    if (frame.lineSpacing) {
        result["lineSpacing"] = *frame.lineSpacing;
    }
    result["showBorder"] = frame.showBorder;
    result["borderThickness"] = frame.borderThickness;
    result["cornerRadius"] = frame.cornerRadius;
    result["positionFromFrameEdge"] = frame.positionFromFrameEdge;
    result["hasCustomShape"] = frame.hasCustomShape;
    return result;
}

json measureTextJson(const classify::MeasureTextClassification& text)
{
    return {
        {"unit", "evpu"},
        {"horizontalOffset", text.horizontalOffset},
        {"verticalOffset", text.verticalOffset},
        {"verticalOrigin", "staff-top-line"},
        {"frame", textBlockFrameJson(text.frame)},
    };
}

json pageTextJson(const classify::PageTextClassification& text, FontTable& fonts)
{
    const auto& assignment = *text.assignment;
    // A single-page assignment names its page by its cmper; a range names its first and last, the last
    // 0 for through the last page. Both are page assignment ids; see design-decisions.md.
    const auto cmper = assignment.getCmper();
    json result{
        {"text", formattedTextJson(text.text, fonts)},
        {"assignment",
            {
                {"start", cmper ? cmper : assignment.startPage},
                {"end", cmper ? cmper : assignment.endPage},
                {"pages", pageAssignTypeName(assignment.oddEven)},
            }},
        {"unit", "evpu"},
        {"alignment",
            {
                {"horizontal", horizontalAlignmentName(assignment.hPosLp)},
                {"rightPageHorizontal", horizontalAlignmentName(assignment.hPosRp)},
                {"vertical", verticalAlignmentName(assignment.vPos)},
                {"horizontalFrom", assignment.hPosPageEdge ? "page-edge" : "margin"},
                {"verticalFrom", assignment.vPosPageEdge ? "page-edge" : "margin"},
            }},
        {"position", pageTextPositionJson(assignment)},
    };
    result["frame"] = textBlockFrameJson(text.frame);
    return result;
}

} // namespace gap_report
} // namespace denigma
