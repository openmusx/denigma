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
#include "denigma/gap_report.h"

#include <string>
#include <type_traits>

#include "nlohmann/json.hpp"

namespace denigma {

namespace {

using json = nlohmann::ordered_json;

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
            {"position", classify::chordSuffixStringPositionName(string.position)},
        });
    }
    auto degrees = json::array();
    for (const auto& degree : suffix.degrees) {
        degrees.push_back({
            {"value", degree.value},
            {"alteration", degree.alteration},
            {"type", classify::chordDegreeTypeName(degree.type)},
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
        result["quality"] = classify::chordQualityName(*suffix.quality);
    }
    return result;
}

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
        result["bassArrangement"] = classify::chordBassArrangementName(*chord.bassArrangement);
    }
    return result;
}

json noteheadJson(const classify::NoteheadClassification& notehead)
{
    json result{
        {"shape", classify::noteheadShapeName(notehead.shape)},
        {"fill", classify::noteheadFillName(notehead.fill)},
    };
    if (notehead.glyphName) {
        result["glyph"] = *notehead.glyphName;
    }
    return result;
}

json gapJson(const classify::Gap& gap)
{
    json result{
        {"anchor", gap.anchor.id},
    };
    if (gap.anchor.staff) {
        result["staff"] = *gap.anchor.staff;
    }
    if (gap.anchor.position) {
        result["position"] = {
            {"numerator", gap.anchor.position->numerator},
            {"denominator", gap.anchor.position->denominator},
        };
    }
    std::visit(
        [&](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, classify::ChordSymbolClassification>) {
                result["type"] = "chord-symbol";
                result["chord"] = chordJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::NoteheadClassification>) {
                result["type"] = "notehead";
                result["notehead"] = noteheadJson(payload);
            }
        },
        gap.payload);
    return result;
}

} // namespace

std::string serializeGapReport(const classify::GapCollector& collector, const GapReportProducer& producer)
{
    auto gaps = json::array();
    for (const auto& gap : collector.gaps()) {
        gaps.push_back(gapJson(gap));
    }
    return json{
        {"schemaVersion", 1},
        {"producer", {{"name", producer.name}, {"version", producer.version}, {"commit", producer.commit}}},
        {"gaps", std::move(gaps)},
    }
        .dump(2);
}

} // namespace denigma
