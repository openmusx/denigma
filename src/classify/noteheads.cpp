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
#include "denigma/classify/noteheads.h"

#include <string_view>
#include <unordered_map>

#include "smufl_mapping.h"

namespace denigma {
namespace classify {

using namespace notehead;

namespace {

NoteheadClassification makeNotehead(Shape shape, Fill fill, std::optional<std::string> glyphName, bool glyphPreferred = false)
{
    NoteheadClassification result;
    result.shape = shape;
    result.fill = fill;
    result.glyphName = std::move(glyphName);
    result.glyphPreferred = glyphPreferred;
    return result;
}

/// A glyph's shape family and fill, and whether it differs from the standard glyph of that shape.
struct GlyphTraits
{
    Shape shape;
    Fill fill;
    bool glyphPreferred = false;
};

NoteheadClassification classifyGlyphName(std::string glyphName)
{
    static const std::unordered_map<std::string_view, GlyphTraits> glyphTable = {
        // Null
        {"noteheadNull", {Shape::Null, Fill::Unspecified}},

        // Regular
        {"noteheadBlack", {Shape::Regular, Fill::Filled}},
        {"noteheadHalfFilled", {Shape::Regular, Fill::Filled}},
        {"noteheadWholeFilled", {Shape::Regular, Fill::Filled}},
        {"noteheadHalf", {Shape::Regular, Fill::Unfilled}},
        {"noteheadWhole", {Shape::Regular, Fill::Unfilled}},
        {"noteheadDoubleWhole", {Shape::Regular, Fill::Unfilled}},
        {"noteheadDoubleWholeSquare", {Shape::Regular, Fill::Unfilled}},

        // X
        {"noteheadXBlack", {Shape::X, Fill::Filled}},
        {"noteheadHeavyX", {Shape::X, Fill::Filled}},
        {"noteheadHeavyXHat", {Shape::X, Fill::Filled}},
        {"noteheadXHalf", {Shape::X, Fill::Unfilled}},
        {"noteheadXWhole", {Shape::X, Fill::Unfilled}},
        {"noteheadXDoubleWhole", {Shape::X, Fill::Unfilled}},

        // Diamond
        {"noteheadDiamondBlack", {Shape::Diamond, Fill::Filled}},
        {"noteheadDiamondBlackOld", {Shape::Diamond, Fill::Filled}},
        {"noteheadDiamondBlackWide", {Shape::Diamond, Fill::Filled}},
        {"noteheadDiamondHalfFilled", {Shape::Diamond, Fill::Filled}},
        {"noteheadDiamondHalf", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondHalfOld", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondHalfWide", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondWhite", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondWhiteWide", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondWhole", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondWholeOld", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondDoubleWhole", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondDoubleWholeOld", {Shape::Diamond, Fill::Unfilled}},
        {"noteheadDiamondOpen", {Shape::Diamond, Fill::Unfilled}},

        // SmallSlash: no small slash is the standard slash.
        {"noteheadSlashVerticalEndsSmall", {Shape::SmallSlash, Fill::Filled, true}},

        // LargeSlash: the standard slash is of normal size with vertical ends.
        {"noteheadSlashVerticalEnds", {Shape::LargeSlash, Fill::Filled}},
        {"noteheadSlashHorizontalEnds", {Shape::LargeSlash, Fill::Filled, true}},
        {"noteheadSlashWhiteWhole", {Shape::LargeSlash, Fill::Unfilled}},
        {"noteheadSlashWhiteHalf", {Shape::LargeSlash, Fill::Unfilled}},
        {"noteheadSlashWhiteDoubleWhole", {Shape::LargeSlash, Fill::Unfilled}},

        // Circled
        {"noteheadCircledBlack", {Shape::Circled, Fill::Filled}},
        {"noteheadCircledHalf", {Shape::Circled, Fill::Unfilled}},
        {"noteheadCircledWhole", {Shape::Circled, Fill::Unfilled}},
        {"noteheadCircledDoubleWhole", {Shape::Circled, Fill::Unfilled}},

        // LargeCircled
        {"noteheadCircledBlackLarge", {Shape::LargeCircled, Fill::Filled}},
        {"noteheadCircledHalfLarge", {Shape::LargeCircled, Fill::Unfilled}},
        {"noteheadCircledWholeLarge", {Shape::LargeCircled, Fill::Unfilled}},
        {"noteheadCircledDoubleWholeLarge", {Shape::LargeCircled, Fill::Unfilled}},

        // CircleX and Plus: their glyphs vary with duration, but the strokes of a cross cannot be filled.
        {"noteheadCircleX", {Shape::CircleX, Fill::Unspecified}},
        {"noteheadCircleXHalf", {Shape::CircleX, Fill::Unspecified}},
        {"noteheadCircleXWhole", {Shape::CircleX, Fill::Unspecified}},
        {"noteheadCircleXDoubleWhole", {Shape::CircleX, Fill::Unspecified}},
        {"noteheadPlusBlack", {Shape::Plus, Fill::Unspecified}},
        {"noteheadPlusHalf", {Shape::Plus, Fill::Unspecified}},
        {"noteheadPlusWhole", {Shape::Plus, Fill::Unspecified}},
        {"noteheadPlusDoubleWhole", {Shape::Plus, Fill::Unspecified}},

        // Triangle
        {"noteheadTriangleUpBlack", {Shape::Triangle, Fill::Filled}},
        {"noteheadTriangleUpWhite", {Shape::Triangle, Fill::Unfilled}},
        {"noteheadTriangleUpHalf", {Shape::Triangle, Fill::Unfilled}},
        {"noteheadTriangleUpWhole", {Shape::Triangle, Fill::Unfilled}},
        {"noteheadTriangleUpDoubleWhole", {Shape::Triangle, Fill::Unfilled}},

        // TriangleDown
        {"noteheadTriangleDownBlack", {Shape::TriangleDown, Fill::Filled}},
        {"noteheadTriangleDownWhite", {Shape::TriangleDown, Fill::Unfilled}},
        {"noteheadTriangleDownHalf", {Shape::TriangleDown, Fill::Unfilled}},
        {"noteheadTriangleDownWhole", {Shape::TriangleDown, Fill::Unfilled}},
        {"noteheadTriangleDownDoubleWhole", {Shape::TriangleDown, Fill::Unfilled}},

        // Square
        {"noteheadSquareBlack", {Shape::Square, Fill::Filled}},
        {"noteheadSquareBlackLarge", {Shape::Square, Fill::Filled}},
        {"noteheadSquareWhite", {Shape::Square, Fill::Unfilled}},
        {"noteheadSquareBlackWhite", {Shape::Square, Fill::Unfilled}},

        // Round: the standard round noteheads are the plain black and white ones.
        {"noteheadRoundBlack", {Shape::Round, Fill::Filled}},
        {"noteheadRoundWhite", {Shape::Round, Fill::Unfilled}},
        {"noteheadRoundBlackDoubleSlashed", {Shape::Round, Fill::Filled, true}},
        {"noteheadRoundBlackLarge", {Shape::Round, Fill::Filled, true}},
        {"noteheadRoundBlackSlashed", {Shape::Round, Fill::Filled, true}},
        {"noteheadRoundBlackSlashedLarge", {Shape::Round, Fill::Filled, true}},
        {"noteheadRoundWhiteDoubleSlashed", {Shape::Round, Fill::Unfilled, true}},
        {"noteheadRoundWhiteLarge", {Shape::Round, Fill::Unfilled, true}},
        {"noteheadRoundWhiteSlashed", {Shape::Round, Fill::Unfilled, true}},
        {"noteheadRoundWhiteSlashedLarge", {Shape::Round, Fill::Unfilled, true}},
        {"noteheadRoundWhiteWithDot", {Shape::Round, Fill::Unfilled, true}},
        {"noteheadRoundWhiteWithDotLarge", {Shape::Round, Fill::Unfilled, true}},

        // Slashed (bottom left to top right) and BackSlashed (top left to bottom right)
        {"noteheadSlashedBlack1", {Shape::Slashed, Fill::Filled}},
        {"noteheadSlashedHalf1", {Shape::Slashed, Fill::Unfilled}},
        {"noteheadSlashedWhole1", {Shape::Slashed, Fill::Unfilled}},
        {"noteheadSlashedDoubleWhole1", {Shape::Slashed, Fill::Unfilled}},
        {"noteheadSlashedBlack2", {Shape::BackSlashed, Fill::Filled}},
        {"noteheadSlashedHalf2", {Shape::BackSlashed, Fill::Unfilled}},
        {"noteheadSlashedWhole2", {Shape::BackSlashed, Fill::Unfilled}},
        {"noteheadSlashedDoubleWhole2", {Shape::BackSlashed, Fill::Unfilled}},
    };

    const std::string_view glyph = glyphName;
    if (const auto it = glyphTable.find(glyph); it != glyphTable.end()) {
        return makeNotehead(it->second.shape, it->second.fill, std::move(glyphName), it->second.glyphPreferred);
    }
    // Any glyph Finale draws as a notehead is one, whether or not SMuFL files it among its noteheads:
    // shape-note and note-name glyphs are named otherwise, and a user may choose any glyph at all.
    // No shape describes it, so only its glyph can.
    return makeNotehead(Shape::Other, Fill::Unspecified, std::move(glyphName), true);
}

/// Recognizes a literal ASCII 'x'/'X' as a cross notehead, but only in a font that is not a symbol
/// font (where such a codepoint would otherwise be a direct glyph-index reference to something else).
NoteheadClassification classifyAsciiX(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    if (!fontInfo || fontInfo->calcIsSymbolFont()) {
        return {};
    }
    if (symbol == U'x' || symbol == U'X') {
        return makeNotehead(Shape::X, Fill::Filled, std::nullopt);
    }
    return {};
}

} // namespace

NoteheadClassification classifyNoteheadSymbol(const musx::dom::MusxInstance<musx::dom::FontInfo>& fontInfo, char32_t symbol)
{
    if (fontInfo) {
        symbol = fontInfo->calcDrawnCharacter(symbol);
    }
    NoteheadClassification result;
    if (symbol == U' ') {
        result = makeNotehead(Shape::Null, Fill::Unspecified, std::nullopt);
    } else if (auto asciiClassification = classifyAsciiX(fontInfo, symbol)) {
        result = std::move(asciiClassification);
    } else if (fontInfo) {
        if (const auto* glyphName =
                smufl_mapping::getGlyphNameForFont(fontInfo->getName(), symbol, fontInfo->calcIsSMuFL(), smufl_mapping::SmuflGlyphSource::Finale)) {
            result = classifyGlyphName(std::string(*glyphName));
        }
    }
    result.noteheadInfo.font = fontInfo;
    result.noteheadInfo.character = symbol;
    return result;
}

NoteheadClassification classifyNotehead(const musx::dom::NoteInfoPtr& note)
{
    if (!note) {
        return {};
    }
    const auto noteheadInfo = note.calcNoteheadInfo();
    auto result = classifyNoteheadSymbol(noteheadInfo.font, noteheadInfo.character);
    result.noteheadInfo = noteheadInfo;
    return result;
}

bool NoteheadClassification::calcFillOverridesDefault(musx::dom::NoteType noteType) const noexcept
{
    const bool defaultsToFilled = musx::dom::Edu(noteType) <= musx::dom::Edu(musx::dom::NoteType::Quarter);
    return (fill == notehead::Fill::Filled && !defaultsToFilled) || (fill == notehead::Fill::Unfilled && defaultsToFilled);
}

bool NoteheadClassification::calcOverridesDefault(musx::dom::NoteType noteType) const noexcept
{
    // An unrecognized glyph overrides the default only when some setting other than the document's
    // default notehead supplied it; otherwise it is the default notehead in a font with no glyph mapping.
    if (shape == notehead::Shape::Unclassified) {
        return noteheadInfo.source != musx::dom::NoteInfoPtr::NoteheadInfo::Source::Default;
    }
    return calcDiffersFromFamily(notehead::Shape::Regular, noteType);
}

bool NoteheadClassification::calcDiffersFromFamily(notehead::Shape family, musx::dom::NoteType noteType) const noexcept
{
    return shape != family || calcFillOverridesDefault(noteType);
}

PercussionNoteheadClassification classifyPercussionNotehead(const musx::dom::MusxInstance<musx::dom::others::PercussionNoteInfo>& percussionNote)
{
    if (!percussionNote) {
        return {};
    }
    const auto fontInfo = [&]() -> musx::dom::MusxInstance<musx::dom::FontInfo> {
        if (const auto fontOptions = percussionNote->getDocument()->getOptions()->get<musx::dom::options::FontOptions>()) {
            return fontOptions->getFontInfo(musx::dom::options::FontOptions::FontType::Percussion);
        }
        return nullptr;
    }();
    const auto classifySlot = [&](char32_t symbol) {
        auto slot = classifyNoteheadSymbol(fontInfo, symbol);
        slot.noteheadInfo.source = musx::dom::NoteInfoPtr::NoteheadInfo::Source::PercussionMap;
        return slot;
    };
    PercussionNoteheadClassification result;
    result.quarterAndShorter = classifySlot(percussionNote->closedNotehead);
    result.half = classifySlot(percussionNote->halfNotehead);
    result.whole = classifySlot(percussionNote->wholeNotehead);
    result.breveAndLonger = classifySlot(percussionNote->dwholeNotehead);
    result.family = result.quarterAndShorter.shape;
    return result;
}

} // namespace classify
} // namespace denigma
