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
#include <string>
#include <string_view>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace notehead {

/// @enum Shape
/// @brief Notehead shape families recognized by Denigma's classifier.
enum class Shape {
    Unclassified,
    /// The notehead is a known SMuFL glyph, named in #NoteheadClassification::glyphName, that does not
    /// belong to one of the specifically recognized shape families below. Any glyph qualifies.
    Other,
    Null,
    Regular,
    X,
    Diamond,
    SmallSlash,
    LargeSlash,
    /// A notehead drawn inside a circle.
    Circled,
    /// An X enclosed in a circle.
    CircleX,
    /// A plus sign.
    Plus,
    /// A triangle pointing up, irrespective of stem direction.
    Triangle,
    /// A triangle pointing down, irrespective of stem direction.
    TriangleDown,
    /// A square.
    Square
};

/// @enum Fill
/// @brief Distinguishes a filled (solid) notehead glyph from an unfilled (open/outline) one.
enum class Fill {
    /// Fill state is not known or not applicable (e.g. for #Shape::Unclassified or #Shape::Other).
    Unspecified,
    Filled,
    Unfilled
};

} // namespace notehead

/// @struct NoteheadClassification
/// @brief Result returned by notehead classification.
struct NoteheadClassification
{
    /// Classified notehead shape family.
    notehead::Shape shape{notehead::Shape::Unclassified};
    /// Filled/unfilled state of the glyph. #notehead::Fill::Unspecified when not known or not applicable.
    notehead::Fill fill{notehead::Fill::Unspecified};
    /// SMuFL glyph name associated with the recognized symbol, when available.
    std::optional<std::string> glyphName;
    /// The resolved font, character, size, and offset that produced this classification.
    musx::dom::NoteInfoPtr::NoteheadInfo noteheadInfo;

    /// Returns true when the source was recognized as a notehead.
    explicit operator bool() const noexcept { return shape != notehead::Shape::Unclassified; }

    /// Returns true when this classification changes the notehead a target would infer from the note duration.
    /// An unrecognized glyph counts only when #noteheadInfo reports a source other than the document default.
    bool calcOverridesDefault(musx::dom::NoteType noteType) const noexcept;

    /// Returns true when the classified fill changes the fill inferred from the note duration.
    bool calcFillOverridesDefault(musx::dom::NoteType noteType) const noexcept;

    /// Returns true when this classification is not the notehead a note of @p noteType would draw if it
    /// used the shape @p family with the fill inferred from its duration.
    bool calcDiffersFromFamily(notehead::Shape family, musx::dom::NoteType noteType) const noexcept;
};

/// @struct PercussionNoteheadClassification
/// @brief Result returned by percussion notehead classification.
///
/// A percussion note specifies one notehead for each of four duration ranges. Its family is the shape of
/// the notehead for quarter notes and shorter, which draws nearly every note that uses it. The other three
/// do not affect the family: a percussion note whose longer noteheads have a different shape, such as an X
/// notehead that becomes a circled X on half notes and longer, still belongs to the family of its quarter
/// notehead. Compare a note's own classification with #family to find the notes that differ from it.
struct PercussionNoteheadClassification
{
    /// Notehead shape family of the percussion note. Unclassified when the quarter notehead is unclassified.
    notehead::Shape family{notehead::Shape::Unclassified};
    /// Notehead for quarter notes and shorter.
    NoteheadClassification quarterAndShorter;
    /// Notehead for half notes.
    NoteheadClassification half;
    /// Notehead for whole notes.
    NoteheadClassification whole;
    /// Notehead for breves and longer.
    NoteheadClassification breveAndLonger;

    /// Returns true when the percussion note's notehead family was recognized.
    explicit operator bool() const noexcept { return family != notehead::Shape::Unclassified; }
};

/// Classifies a notehead symbol from a font and character code, as the character the font draws.
/// (See #musx::dom::FontInfo::calcDrawnCharacter.)
NoteheadClassification classifyNoteheadSymbol(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol);

/// Classifies the effective notehead for a note, resolving the note's font/character
/// via #musx::dom::NoteInfoPtr::calcNoteheadInfo.
NoteheadClassification classifyNotehead(const musx::dom::NoteInfoPtr& note);

/// Classifies the noteheads of a percussion note, drawn from the document's percussion notehead font.
PercussionNoteheadClassification classifyPercussionNotehead(const musx::dom::MusxInstance<musx::dom::others::PercussionNoteInfo>& percussionNote);

} // namespace classify
} // namespace denigma
