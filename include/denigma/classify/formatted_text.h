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

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace text {

/// @struct Insert
/// @brief The Enigma text insert a run was substituted for.
///
/// The runs contain the substitution musxdom made. A consumer that renders the insert itself
/// replaces together the runs sharing one Run::sourceChunk.
struct Insert
{
    /// @enum Kind
    /// @brief The insert's meaning, for the inserts the classifier names.
    enum class Kind {
        Other, ///< An insert the classifier does not name; #command and #parameters still identify it.
        Accidental, ///< An accidental symbol: `^flat`, `^sharp`, `^natural`, `^dbflat`, `^dbsharp`.
        PlaybackValue, ///< The expression's playback value: `^value`.
        PlaybackController, ///< The expression's playback controller value: `^control`.
        PlaybackPass, ///< The expression's repeat pass: `^pass`.
        RehearsalMark ///< The rehearsal mark's sequence text: `^rehearsal`.
    };

    /// @brief The insert's meaning.
    Kind kind{Kind::Other};
    /// @brief The insert as written, without the leading caret.
    std::string command;
    /// @brief The insert's parameters as written.
    std::vector<std::string> parameters;
};

/// @struct Run
/// @brief One stretch of formatted text in a single font, holding either text or music glyphs.
struct Run
{
    /// @brief Visible UTF-8 text of the run. For a glyph run, the source characters the glyphs came from.
    std::string text;
    /// @brief Document-independent snapshot of the run's font.
    musx::dom::ResolvedFontInfo font;
    /// @brief One SMuFL glyph name per code point when the run is music glyphs; empty for a text run.
    std::vector<std::string> glyphNames;
    /// @brief The insert the run was substituted for, when it was. Split runs from one substitution share #sourceChunk.
    std::optional<Insert> insert;

    /// @brief Baseline and superscript offsets in EVPU, and tracking in thousandths of an em.
    musx::dom::Evpu baseline{};
    musx::dom::Evpu superscript{};
    int tracking{};
    /// @brief Parser chunk ordinal; split runs from one substitution share this value.
    size_t sourceChunk{};

    /// @brief True when the run is music glyphs rather than text.
    [[nodiscard]] bool isGlyphRun() const noexcept { return !glyphNames.empty(); }
};

} // namespace text

/// @struct FormattedText
/// @brief Document-independent classification of a formatted Enigma text.
///
/// Every value is a snapshot, so an instance remains valid after the document that produced it
/// is released. Hidden and empty chunks are dropped. Within a chunk, characters that resolve to
/// SMuFL glyph names and characters that do not form separate runs, so that a reader lacking the
/// source font can still draw the glyphs by name; the runs otherwise keep source order and fonts.
/// An accidental insert is a glyph run named by its SMuFL accidental, with the Unicode accidental
/// character as its text.
struct FormattedText
{
    /// @brief Visible runs in source order.
    std::vector<text::Run> runs;
    /// @brief The runs' text joined, with Unicode accidental characters.
    std::string plainText;
};

/// @brief Classifies a formatted Enigma text.
/// @param text Parsing context of the source text.
FormattedText classifyFormattedText(const musx::util::EnigmaParsingContext& text);

} // namespace classify
} // namespace denigma
