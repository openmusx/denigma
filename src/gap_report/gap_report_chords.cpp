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

#include <string>
#include <utility>

namespace denigma {
namespace gap_report {

namespace {

using namespace classify;

// Reporting names of the classifier enums this serializer writes.

std::string_view chordQualityName(chord::Quality quality)
{
    using Quality = chord::Quality;
    switch (quality) {
    case Quality::Major: return "major";
    case Quality::Minor: return "minor";
    case Quality::Augmented: return "augmented";
    case Quality::Diminished: return "diminished";
    case Quality::Dominant: return "dominant";
    case Quality::AugmentedSeventh: return "augmented-seventh";
    case Quality::MajorSeventh: return "major-seventh";
    case Quality::MinorSeventh: return "minor-seventh";
    case Quality::DiminishedSeventh: return "diminished-seventh";
    case Quality::HalfDiminished: return "half-diminished";
    case Quality::MajorMinor: return "major-minor";
    case Quality::MajorSixth: return "major-sixth";
    case Quality::MinorSixth: return "minor-sixth";
    case Quality::DominantNinth: return "dominant-ninth";
    case Quality::MajorNinth: return "major-ninth";
    case Quality::MinorNinth: return "minor-ninth";
    case Quality::DominantEleventh: return "dominant-11th";
    case Quality::MajorEleventh: return "major-11th";
    case Quality::MinorEleventh: return "minor-11th";
    case Quality::DominantThirteenth: return "dominant-13th";
    case Quality::MajorThirteenth: return "major-13th";
    case Quality::MinorThirteenth: return "minor-13th";
    case Quality::SuspendedSecond: return "suspended-second";
    case Quality::SuspendedFourth: return "suspended-fourth";
    case Quality::Pedal: return "pedal";
    case Quality::None: return "none";
    case Quality::Power: return "power";
    }
    return "unknown";
}

std::string_view chordDegreeTypeName(chord::Degree::Type type)
{
    switch (type) {
    case chord::Degree::Type::Add: return "add";
    case chord::Degree::Type::Remove: return "remove";
    case chord::Degree::Type::Alter: return "alter";
    }
    return "unknown";
}

std::string_view chordBassArrangementName(chord::BassArrangement arrangement)
{
    switch (arrangement) {
    case chord::BassArrangement::Horizontal: return "horizontal";
    case chord::BassArrangement::Vertical: return "vertical";
    case chord::BassArrangement::Diagonal: return "diagonal";
    }
    return "unknown";
}

std::string_view chordSuffixStringPositionName(chord::SuffixString::Position position)
{
    switch (position) {
    case chord::SuffixString::Position::Inline: return "inline";
    case chord::SuffixString::Position::Above: return "above";
    case chord::SuffixString::Position::Below: return "below";
    }
    return "inline";
}

json pitchJson(const music_theory::Pitch& pitch)
{
    return {
        {"step", std::string(1, music_theory::calcNoteNameLetter(pitch.noteName))},
        {"alteration", pitch.alteration},
    };
}

json suffixJson(const classify::ChordSuffixClassification& suffix)
{
    auto strings = json::array();
    for (const auto& string : suffix.strings) {
        strings.push_back({
            {"text", string.text},
            {"position", chordSuffixStringPositionName(string.position)},
        });
    }
    auto degrees = json::array();
    for (const auto& degree : suffix.degrees) {
        degrees.push_back({
            {"value", degree.value},
            {"alteration", degree.alteration},
            {"type", chordDegreeTypeName(degree.type)},
            {"impliedByText", degree.impliedByText},
        });
    }
    json result{
        {"strings", std::move(strings)},
        {"suffixText", suffix.calcText()},
        {"degrees", std::move(degrees)},
        {"parenthesizeDegrees", suffix.parenthesizeDegrees},
        {"stackDegrees", suffix.stackDegrees},
        {"hasOuterParentheses", suffix.hasOuterParentheses},
        {"hasUnrecognizedGlyphs", suffix.hasUnrecognizedGlyphs},
    };
    if (suffix.quality) {
        result["quality"] = chordQualityName(*suffix.quality);
    }
    return result;
}

} // namespace

json chordJson(const classify::ChordSymbolClassification& chord)
{
    json result{
        {"root", pitchJson(chord.root)},
        {"rootLowerCase", chord.rootLowerCase},
        {"showRoot", chord.showRoot},
        {"showSuffix", chord.showSuffix},
        {"suffix", suffixJson(chord.suffix)},
    };
    if (chord.bass) {
        result["bass"] = pitchJson(*chord.bass);
        result["bassLowerCase"] = chord.bassLowerCase;
    }
    if (chord.bassArrangement) {
        result["bassArrangement"] = chordBassArrangementName(*chord.bassArrangement);
    }
    return result;
}

} // namespace gap_report
} // namespace denigma
