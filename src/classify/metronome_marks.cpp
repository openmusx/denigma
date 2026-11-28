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

#include <charconv>
#include <cstddef>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>

#include "classify/classify.h"
#include "utils/stringutils.h"
#include "utils/utf8_iterator.h"

namespace denigma {
namespace classify {
namespace detail {

namespace {

struct MetronomeNote
{
    musx::dom::NoteType noteType{};
    /// The metronome-note glyph naming the same note, which may differ from the source glyph.
    std::string_view glyphName;
};

/// Resolves a note glyph to its value. Legacy fonts usually map their note glyphs to the general
/// `note*` glyphs rather than the `metNote*` ones, so both spellings are accepted.
static std::optional<MetronomeNote> metronomeNoteFromGlyphName(std::string_view glyphName)
{
    using NoteType = musx::dom::NoteType;
    static const std::unordered_map<std::string_view, MetronomeNote> notes = {
        {"metNoteDoubleWhole", {NoteType::Breve, "metNoteDoubleWhole"}},
        {"metNoteDoubleWholeSquare", {NoteType::Breve, "metNoteDoubleWholeSquare"}},
        {"metNoteWhole", {NoteType::Whole, "metNoteWhole"}},
        {"metNoteHalfUp", {NoteType::Half, "metNoteHalfUp"}},
        {"metNoteHalfDown", {NoteType::Half, "metNoteHalfDown"}},
        {"metNoteQuarterUp", {NoteType::Quarter, "metNoteQuarterUp"}},
        {"metNoteQuarterDown", {NoteType::Quarter, "metNoteQuarterDown"}},
        {"metNote8thUp", {NoteType::Eighth, "metNote8thUp"}},
        {"metNote8thDown", {NoteType::Eighth, "metNote8thDown"}},
        {"metNote16thUp", {NoteType::Note16th, "metNote16thUp"}},
        {"metNote16thDown", {NoteType::Note16th, "metNote16thDown"}},
        {"metNote32ndUp", {NoteType::Note32nd, "metNote32ndUp"}},
        {"metNote32ndDown", {NoteType::Note32nd, "metNote32ndDown"}},
        {"metNote64thUp", {NoteType::Note64th, "metNote64thUp"}},
        {"metNote64thDown", {NoteType::Note64th, "metNote64thDown"}},
        {"metNote128thUp", {NoteType::Note128th, "metNote128thUp"}},
        {"metNote128thDown", {NoteType::Note128th, "metNote128thDown"}},
        {"metNote256thUp", {NoteType::Note256th, "metNote256thUp"}},
        {"metNote256thDown", {NoteType::Note256th, "metNote256thDown"}},
        {"metNote512thUp", {NoteType::Note512th, "metNote512thUp"}},
        {"metNote512thDown", {NoteType::Note512th, "metNote512thDown"}},
        {"metNote1024thUp", {NoteType::Note1024th, "metNote1024thUp"}},
        {"metNote1024thDown", {NoteType::Note1024th, "metNote1024thDown"}},
        {"noteDoubleWhole", {NoteType::Breve, "metNoteDoubleWhole"}},
        {"noteDoubleWholeSquare", {NoteType::Breve, "metNoteDoubleWholeSquare"}},
        {"noteWhole", {NoteType::Whole, "metNoteWhole"}},
        {"noteheadWhole", {NoteType::Whole, "metNoteWhole"}},
        {"noteHalfUp", {NoteType::Half, "metNoteHalfUp"}},
        {"noteHalfDown", {NoteType::Half, "metNoteHalfDown"}},
        {"noteQuarterUp", {NoteType::Quarter, "metNoteQuarterUp"}},
        {"noteQuarterDown", {NoteType::Quarter, "metNoteQuarterDown"}},
        {"note8thUp", {NoteType::Eighth, "metNote8thUp"}},
        {"note8thDown", {NoteType::Eighth, "metNote8thDown"}},
        {"note16thUp", {NoteType::Note16th, "metNote16thUp"}},
        {"note16thDown", {NoteType::Note16th, "metNote16thDown"}},
        {"note32ndUp", {NoteType::Note32nd, "metNote32ndUp"}},
        {"note32ndDown", {NoteType::Note32nd, "metNote32ndDown"}},
        {"note64thUp", {NoteType::Note64th, "metNote64thUp"}},
        {"note64thDown", {NoteType::Note64th, "metNote64thDown"}},
        {"note128thUp", {NoteType::Note128th, "metNote128thUp"}},
        {"note128thDown", {NoteType::Note128th, "metNote128thDown"}},
        {"note256thUp", {NoteType::Note256th, "metNote256thUp"}},
        {"note256thDown", {NoteType::Note256th, "metNote256thDown"}},
        {"note512thUp", {NoteType::Note512th, "metNote512thUp"}},
        {"note512thDown", {NoteType::Note512th, "metNote512thDown"}},
        {"note1024thUp", {NoteType::Note1024th, "metNote1024thUp"}},
        {"note1024thDown", {NoteType::Note1024th, "metNote1024thDown"}},
    };
    const auto found = notes.find(glyphName);
    return found != notes.end() ? std::optional{found->second} : std::nullopt;
}

/// Folds a font name the way legacy font names are matched: ASCII case folded, ASCII whitespace
/// removed.
static std::string foldedFontName(std::string_view fontName)
{
    std::string result;
    result.reserve(fontName.size());
    for (const char ch : fontName) {
        if (!utils::isSpace(static_cast<unsigned char>(ch))) {
            result.push_back(utils::toLowerCase(ch));
        }
    }
    return result;
}

/// Resolves a glyph that draws a note and an equals sign together. SMuFL has no such glyph, so
/// these are listed here by font and codepoint. Fonts without a SMuFL mapping are included as
/// well: with ASCII numerals the rest of the equation needs no mapping.
static std::optional<MetronomeNote> noteEqualsGlyph(const musx::dom::FontInfo& font, char32_t codepoint)
{
    using NoteType = musx::dom::NoteType;
    using CodepointNotes = std::unordered_map<char32_t, MetronomeNote>;
    static const CodepointNotes quarterAtF2 = {{0xF2, {NoteType::Quarter, "metNoteQuarterUp"}}};
    static const CodepointNotes graceNotes = {
        {0x64, {NoteType::Quarter, "metNoteQuarterUp"}}, {0x44, {NoteType::Half, "metNoteHalfUp"}}, {0x60, {NoteType::Eighth, "metNote8thUp"}}};
    static const CodepointNotes quarterAt8B = {{0x8B, {NoteType::Quarter, "metNoteQuarterUp"}}};
    static const CodepointNotes quarterAtE3 = {{0xE3, {NoteType::Quarter, "metNoteQuarterUp"}}};
    static const std::unordered_map<std::string_view, const CodepointNotes*> fonts = {
        {"maestro", &quarterAtF2},
        {"maestrowide", &quarterAtF2},
        {"oslo", &quarterAtF2},
        {"paris", &quarterAtF2},
        {"stockholmclassic", &quarterAtF2},
        {"stockholmplate", &quarterAtF2},
        {"vienna", &quarterAtF2},
        {"petrucci", &quarterAtF2},
        {"ashmusic", &quarterAtF2},
        {"leemusic", &quarterAtF2},
        {"russmusic", &quarterAtF2},
        {"november", &quarterAtF2},
        {"sonata", &quarterAtF2},
        {"fjhgracenotes", &graceNotes},
        {"gracenotesflf", &graceNotes},
        {"broadwaycopyisttext", &quarterAt8B},
        {"jazztext", &quarterAt8B},
        {"finalecopyisttext", &quarterAtE3},
    };
    if (font.calcIsSMuFL()) {
        return std::nullopt;
    }
    const auto fontIt = fonts.find(foldedFontName(font.getName()));
    if (fontIt == fonts.end()) {
        return std::nullopt;
    }
    const auto noteIt = fontIt->second->find(font.calcDrawnCharacter(codepoint));
    return noteIt != fontIt->second->end() ? std::optional{noteIt->second} : std::nullopt;
}

/// Returns the digit a SMuFL numeral glyph draws. Legacy fonts often map their small numerals to
/// the tuplet digits and their large ones to the time-signature digits.
static std::optional<char> metronomeDigitFromGlyphName(std::string_view glyphName)
{
    static const std::unordered_map<std::string_view, char> digits = {
        {"tuplet0", '0'},
        {"tuplet1", '1'},
        {"tuplet2", '2'},
        {"tuplet3", '3'},
        {"tuplet4", '4'},
        {"tuplet5", '5'},
        {"tuplet6", '6'},
        {"tuplet7", '7'},
        {"tuplet8", '8'},
        {"tuplet9", '9'},
        {"timeSig0", '0'},
        {"timeSig1", '1'},
        {"timeSig2", '2'},
        {"timeSig3", '3'},
        {"timeSig4", '4'},
        {"timeSig5", '5'},
        {"timeSig6", '6'},
        {"timeSig7", '7'},
        {"timeSig8", '8'},
        {"timeSig9", '9'},
    };
    const auto found = digits.find(glyphName);
    return found != digits.end() ? std::optional{found->second} : std::nullopt;
}

static bool isMetronomeEquals(char32_t codepoint, const std::optional<std::string>& glyphName)
{
    return codepoint == U'=' || (glyphName && *glyphName == "timeSigEquals");
}

static bool isMetronomeAugmentationDotGlyph(std::string_view glyphName)
{
    return glyphName == "metAugmentationDot" || glyphName == "augmentationDot";
}

/// Reads the digits, and at most one decimal point, collected to the right of the equals sign.
/// The fractional digits are accumulated separately so that an arbitrarily long tail cannot
/// overflow the integer conversion.
static std::optional<double> parseDisplayedBeatsPerMinute(std::string_view text)
{
    const auto pointPos = text.find('.');
    const auto integerText = text.substr(0, pointPos);
    unsigned long long integerValue{};
    const auto [end, error] = std::from_chars(integerText.data(), integerText.data() + integerText.size(), integerValue);
    if (error != std::errc{} || end != integerText.data() + integerText.size()) {
        return std::nullopt;
    }
    double result = static_cast<double>(integerValue);
    if (pointPos != std::string_view::npos) {
        double scale = 1.0;
        for (const char digit : text.substr(pointPos + 1)) {
            scale /= 10.0;
            result += static_cast<double>(digit - '0') * scale;
        }
    }
    return result;
}

} // namespace

std::optional<expression::MetronomeMark> recognizeMetronomeMark(const std::vector<musx::util::EnigmaTextChunk>& chunks)
{
    enum class ParseState {
        Note,
        DotsOrEquals,
        BeatsPerMinute,
        TrailingSpace
    };

    ParseState state = ParseState::Note;
    musx::dom::NoteType noteType{musx::dom::NoteType::Quarter};
    std::string noteGlyphName;
    size_t augmentationDots = 0;
    // The note glyph drew the equals sign too, so the number may follow it directly.
    bool equalsImplied = false;
    std::string displayedBeatsPerMinute;
    bool seenDecimalPoint = false;
    bool beatsPerMinuteFromGlyphNames = false;
    musx::dom::MusxInstance<musx::dom::FontInfo> noteFont;
    musx::dom::MusxInstance<musx::dom::FontInfo> beatsPerMinuteFont;
    for (const auto& chunk : chunks) {
        const auto& font = chunk.styles.font;
        if (!font) {
            return std::nullopt;
        }
        for (utils::Utf8Iterator iter(chunk.text); !iter.atEnd(); iter.next()) {
            if (!iter.valid()) {
                return std::nullopt;
            }
            const char32_t codepoint = iter->codepoint;
            if (codepoint < 0x80 && utils::isSpace(static_cast<unsigned char>(codepoint))) {
                if (state == ParseState::BeatsPerMinute && !displayedBeatsPerMinute.empty()) {
                    state = ParseState::TrailingSpace;
                }
                continue;
            }

            if (state == ParseState::TrailingSpace) {
                return std::nullopt;
            }

            if (state == ParseState::Note) {
                // Checked before the SMuFL mapping, which may name only the note half of such a glyph.
                auto note = noteEqualsGlyph(*font, codepoint);
                equalsImplied = note.has_value();
                if (!note) {
                    const auto glyphName = detail::glyphNameForFont(font, codepoint);
                    note = glyphName ? metronomeNoteFromGlyphName(*glyphName) : std::nullopt;
                }
                if (!note) {
                    return std::nullopt;
                }
                noteType = note->noteType;
                noteGlyphName = std::string(note->glyphName);
                noteFont = font;
                state = ParseState::DotsOrEquals;
                continue;
            }
            const bool isAsciiDigit = codepoint >= U'0' && codepoint <= U'9';
            const auto glyphName = detail::glyphNameForFont(font, codepoint);
            const auto glyphDigit = !isAsciiDigit && glyphName ? metronomeDigitFromGlyphName(*glyphName) : std::nullopt;
            if (state == ParseState::DotsOrEquals) {
                if (codepoint == U'.' || (glyphName && isMetronomeAugmentationDotGlyph(*glyphName))) {
                    ++augmentationDots;
                    continue;
                }
                if (isMetronomeEquals(codepoint, glyphName)) {
                    state = ParseState::BeatsPerMinute;
                    continue;
                }
                // Dots belong before the equals sign, so a number may follow the glyph directly
                // only when no dot was read.
                if (!equalsImplied || augmentationDots > 0 || (!isAsciiDigit && !glyphDigit)) {
                    return std::nullopt;
                }
                state = ParseState::BeatsPerMinute;
            }
            if (codepoint == U'.') {
                if (seenDecimalPoint || displayedBeatsPerMinute.empty()) {
                    return std::nullopt;
                }
                seenDecimalPoint = true;
                displayedBeatsPerMinute.push_back('.');
                continue;
            }
            if (!isAsciiDigit && !glyphDigit) {
                return std::nullopt;
            }
            if (displayedBeatsPerMinute.empty()) {
                beatsPerMinuteFont = font;
            }
            if (glyphDigit) {
                beatsPerMinuteFromGlyphNames = true;
                displayedBeatsPerMinute.push_back(*glyphDigit);
            } else {
                displayedBeatsPerMinute.push_back(static_cast<char>(codepoint));
            }
        }
    }
    if ((state != ParseState::BeatsPerMinute && state != ParseState::TrailingSpace) || displayedBeatsPerMinute.empty()
        || displayedBeatsPerMinute.back() == '.') {
        return std::nullopt;
    }

    const auto displayedValue = parseDisplayedBeatsPerMinute(displayedBeatsPerMinute);
    if (!displayedValue) {
        return std::nullopt;
    }
    return expression::MetronomeMark{{}, noteType, std::move(noteGlyphName), augmentationDots, *displayedValue, std::move(noteFont),
        std::move(beatsPerMinuteFont), beatsPerMinuteFromGlyphNames};
}

} // namespace detail
} // namespace classify
} // namespace denigma
