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
#include "mnx_formatted_text.h"

#include <tuple>
#include <utility>
#include <vector>

#include "core/element_ids.h"
#include "utils/utf8_iterator.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

void setFormattedText(mnxdom::FormattedText dst, const classify::FormattedText& src, const std::string& textId, classify::GapCollector* gaps)
{
    struct OutputChunk
    {
        std::string text;
        std::vector<std::string> glyphs;
        size_t length{};
        bool isGlyph{};
    };
    std::vector<OutputChunk> chunks;
    std::vector<classify::GapTextRange> ranges;
    for (const auto& run : src.runs) {
        if (chunks.empty() || chunks.back().isGlyph != run.isGlyphRun()) {
            chunks.push_back({{}, {}, 0, run.isGlyphRun()});
        }
        auto& chunk = chunks.back();
        size_t length = run.glyphNames.size();
        if (!run.isGlyphRun()) {
            for (utils::Utf8Iterator iter(run.text); !iter.atEnd(); iter.next()) {
                ++length;
            }
        }
        ranges.push_back({core::calcTextChunkId(textId, chunks.size() - 1), chunk.length, length});
        chunk.length += length;
        chunk.text += run.text;
        chunk.glyphs.insert(chunk.glyphs.end(), run.glyphNames.begin(), run.glyphNames.end());
    }

    dst.clear();
    for (size_t index = 0; index < chunks.size(); ++index) {
        const auto& chunk = chunks[index];
        const auto id = core::calcTextChunkId(textId, index);
        if (chunk.isGlyph) {
            dst.appendSmufl(chunk.glyphs).set_id(id);
        } else {
            dst.appendText(chunk.text).set_id(id);
        }
    }
    if (!gaps) {
        return;
    }

    const auto fontKey = [](const musx::dom::ResolvedFontInfo& font) {
        return std::tie(font.name, font.size, font.sizeIsPercent, font.bold, font.italic, font.underline, font.strikeout, font.absolute, font.hidden,
            font.isSymbolFont, font.isSmufl);
    };
    const auto sameStyle = [&](const classify::text::Run& lhs, const classify::text::Run& rhs) {
        return fontKey(lhs.font) == fontKey(rhs.font) && lhs.baseline == rhs.baseline && lhs.superscript == rhs.superscript
               && lhs.tracking == rhs.tracking;
    };
    const classify::GapAnchor anchor{textId, std::nullopt, std::nullopt};
    for (size_t first = 0; first < src.runs.size();) {
        auto range = ranges[first];
        const auto& run = src.runs[first];
        classify::TextFormatting formatting{run.font, run.baseline, run.superscript, run.tracking, run.text};
        size_t next = first + 1;
        while (next < src.runs.size() && ranges[next].id == range.id && sameStyle(run, src.runs[next])) {
            range.length += ranges[next].length;
            formatting.sourceText += src.runs[next].text;
            ++next;
        }
        gaps->add(anchor, std::move(formatting), classify::GapExtent::Partial, {}, {std::move(range)});
        first = next;
    }
    for (size_t first = 0; first < src.runs.size();) {
        const auto& run = src.runs[first];
        if (!run.insert) {
            ++first;
            continue;
        }
        classify::TextInsert insert{*run.insert, run.text};
        std::vector<classify::GapTextRange> insertRanges{ranges[first]};
        size_t next = first + 1;
        while (next < src.runs.size() && src.runs[next].sourceChunk == run.sourceChunk) {
            insert.sourceText += src.runs[next].text;
            insertRanges.push_back(ranges[next]);
            ++next;
        }
        gaps->add(anchor, std::move(insert), classify::GapExtent::Partial, {}, std::move(insertRanges));
        first = next;
    }
}

mnxdom::FormattedText makeFormattedText(const musx::util::EnigmaParsingContext& src, const std::string& textId, classify::GapCollector* gaps)
{
    mnxdom::FormattedText result;
    setFormattedText(result, classify::classifyFormattedText(src), textId, gaps);
    return result;
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
