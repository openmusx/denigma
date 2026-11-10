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
#include "gap_report_json.h"

#include <cstdint>

namespace denigma {
namespace gap_report {

namespace {

using namespace classify;

// Reporting names of the classifier enums this serializer writes.

std::string_view noteheadShapeName(notehead::Shape shape)
{
    switch (shape) {
    case notehead::Shape::Unclassified: return "unclassified";
    case notehead::Shape::Other: return "other";
    case notehead::Shape::Null: return "null";
    case notehead::Shape::Regular: return "regular";
    case notehead::Shape::X: return "x";
    case notehead::Shape::Diamond: return "diamond";
    case notehead::Shape::SmallSlash: return "small-slash";
    case notehead::Shape::LargeSlash: return "large-slash";
    case notehead::Shape::Circled: return "circled";
    case notehead::Shape::CircleX: return "circle-x";
    case notehead::Shape::Plus: return "plus";
    case notehead::Shape::Triangle: return "triangle";
    case notehead::Shape::TriangleDown: return "triangle-down";
    case notehead::Shape::Square: return "square";
    }
    return "unclassified";
}

std::string_view noteheadFillName(notehead::Fill fill)
{
    switch (fill) {
    case notehead::Fill::Unspecified: return "unspecified";
    case notehead::Fill::Filled: return "filled";
    case notehead::Fill::Unfilled: return "unfilled";
    }
    return "unspecified";
}

} // namespace

json noteheadJson(const classify::NoteheadClassification& notehead)
{
    json result{
        {"shape", noteheadShapeName(notehead.shape)},
        {"fill", noteheadFillName(notehead.fill)},
    };
    if (notehead.glyphName) {
        result["glyph"] = *notehead.glyphName;
    }
    // The source character identifies the notehead even when no glyph name does.
    result["codePoint"] = static_cast<std::uint32_t>(notehead.noteheadInfo.character);
    if (notehead.noteheadInfo.font) {
        result["font"] = fontJson(notehead.noteheadInfo.font->resolve());
    }
    return result;
}

} // namespace gap_report
} // namespace denigma
