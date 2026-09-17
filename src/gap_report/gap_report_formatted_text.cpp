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

#include <string_view>
#include <utility>

namespace denigma {
namespace gap_report {

namespace {

// Reporting names of the classifier enums this serializer writes.

std::string_view insertKindName(classify::text::Insert::Kind kind)
{
    using Kind = classify::text::Insert::Kind;
    switch (kind) {
    case Kind::Other: return "other";
    case Kind::Accidental: return "accidental";
    case Kind::PlaybackValue: return "playback-value";
    case Kind::PlaybackController: return "playback-controller";
    case Kind::PlaybackPass: return "playback-pass";
    case Kind::RehearsalMark: return "rehearsal-mark";
    }
    return "unknown";
}

json fontJson(const musx::dom::ResolvedFontInfo& font)
{
    return {
        {"name", font.name},
        {"size", font.size},
        {"sizeIsPercent", font.sizeIsPercent},
        {"bold", font.bold},
        {"italic", font.italic},
        {"underline", font.underline},
        {"strikeout", font.strikeout},
        {"absolute", font.absolute},
        {"hidden", font.hidden},
        {"isSymbolFont", font.isSymbolFont},
        {"isSmufl", font.isSmufl},
    };
}

} // namespace

json formattedTextJson(const classify::FormattedText& text)
{
    auto runs = json::array();
    for (const auto& run : text.runs) {
        json item{
            {"text", run.text},
            {"font", fontJson(run.font)},
        };
        if (run.isGlyphRun()) {
            item["glyphs"] = run.glyphNames;
        }
        if (run.insert) {
            item["insert"] = {
                {"kind", insertKindName(run.insert->kind)},
                {"command", run.insert->command},
                {"parameters", run.insert->parameters},
            };
        }
        runs.push_back(std::move(item));
    }
    return {
        {"plain", text.plainText},
        {"runs", std::move(runs)},
    };
}

} // namespace gap_report
} // namespace denigma
