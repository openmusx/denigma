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
#include "denigma/classify/formatted_text.h"

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "classify/classify.h"
#include "core/denigma.h"
#include "utils/utf8_iterator.h"

namespace denigma {
namespace classify {

namespace {

/// @brief Names the insert, and for an accidental the SMuFL glyph that draws it.
std::pair<text::Insert::Kind, std::optional<std::string>> classifyInsert(const std::string& command)
{
    using Kind = text::Insert::Kind;
    static const std::unordered_map<std::string_view, std::pair<Kind, std::optional<std::string>>> inserts{
        {"flat", {Kind::Accidental, "accidentalFlat"}},
        {"sharp", {Kind::Accidental, "accidentalSharp"}},
        {"natural", {Kind::Accidental, "accidentalNatural"}},
        {"dbflat", {Kind::Accidental, "accidentalDoubleFlat"}},
        {"dbsharp", {Kind::Accidental, "accidentalDoubleSharp"}},
        {"value", {Kind::PlaybackValue, std::nullopt}},
        {"control", {Kind::PlaybackController, std::nullopt}},
        {"pass", {Kind::PlaybackPass, std::nullopt}},
        {"rehearsal", {Kind::RehearsalMark, std::nullopt}},
    };
    const auto found = inserts.find(command);
    return found != inserts.end() ? found->second : std::pair<Kind, std::optional<std::string>>{Kind::Other, std::nullopt};
}

} // namespace

FormattedText classifyFormattedText(const musx::util::EnigmaParsingContext& text)
{
    FormattedText result;
    ASSERT_IF (!text) {
        return result;
    }
    const auto chunks =
        text.collectEnigmaTextChunks(musx::util::EnigmaString::EnigmaParsingOptions(musx::util::EnigmaString::AccidentalStyle::Unicode));
    for (size_t chunkIndex = 0; chunkIndex < chunks.size(); ++chunkIndex) {
        const auto& chunk = chunks[chunkIndex];
        if (!chunk.styles.font || chunk.styles.font->hidden || chunk.text.empty()) {
            continue;
        }
        const auto resolved = chunk.resolve();
        std::optional<text::Insert> insert;
        std::optional<std::string> accidentalGlyph;
        if (chunk.insert) {
            auto classified = classifyInsert(chunk.insert->command);
            insert = text::Insert{classified.first, chunk.insert->command, chunk.insert->parameters};
            accidentalGlyph = std::move(classified.second);
        }
        const auto appendRun = [&]() -> text::Run& {
            result.runs.push_back(
                {{}, resolved.styles.font, {}, insert, resolved.styles.baseline, resolved.styles.superscript, resolved.styles.tracking, chunkIndex});
            return result.runs.back();
        };
        if (accidentalGlyph) {
            auto& run = appendRun();
            run.text = chunk.text;
            run.glyphNames.push_back(std::move(*accidentalGlyph));
        } else {
            bool chunkStart = true;
            for (utils::Utf8Iterator iter(chunk.text); !iter.atEnd(); iter.next()) {
                auto glyphName = detail::glyphNameForFont(chunk.styles.font, iter->codepoint);
                const bool isGlyph = glyphName.has_value();
                if (chunkStart || result.runs.back().isGlyphRun() != isGlyph) {
                    appendRun();
                }
                chunkStart = false;
                result.runs.back().text.append(chunk.text, iter.offset(), iter->byteCount);
                if (glyphName) {
                    result.runs.back().glyphNames.push_back(std::move(*glyphName));
                }
            }
        }
        result.plainText += chunk.text;
    }
    return result;
}

} // namespace classify
} // namespace denigma
