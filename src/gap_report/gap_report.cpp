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
#include <string_view>
#include <type_traits>
#include <utility>

#include "gap_report_json.h"

namespace denigma {

namespace {

using gap_report::json;

std::string_view gapExtentName(classify::GapExtent extent)
{
    switch (extent) {
    case classify::GapExtent::Complete: return "complete";
    case classify::GapExtent::Partial: return "partial";
    }
    return "unknown";
}

std::string_view gapPlacementKindName(classify::GapPlacement::Kind kind)
{
    using Kind = classify::GapPlacement::Kind;
    switch (kind) {
    case Kind::Staff: return "staff";
    case Kind::SystemTop: return "system-top";
    case Kind::SystemBottom: return "system-bottom";
    }
    return "unknown";
}

json anchorJson(const classify::GapAnchor& anchor)
{
    json result{
        {"anchor", anchor.id},
    };
    if (anchor.staff) {
        result["staff"] = *anchor.staff;
    }
    if (anchor.position) {
        result["position"] = {
            {"numerator", anchor.position->numerator()},
            {"denominator", anchor.position->denominator()},
        };
    }
    return result;
}

json gapJson(const classify::Gap& gap, gap_report::ArrowheadTable& arrowheads)
{
    json result = anchorJson(gap.anchor);
    if (gap.end) {
        result["end"] = anchorJson(*gap.end);
    }
    result["extent"] = gapExtentName(gap.extent);
    if (!gap.placements.empty()) {
        auto placements = json::array();
        for (const auto& placement : gap.placements) {
            json item{{"kind", gapPlacementKindName(placement.kind)}};
            item.update(anchorJson(placement.anchor));
            placements.push_back(std::move(item));
        }
        result["placements"] = std::move(placements);
    }
    std::visit(
        [&](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, classify::ChordSymbolClassification>) {
                result["type"] = "chord-symbol";
                result["chord"] = gap_report::chordJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::NoteheadClassification>) {
                result["type"] = "notehead";
                result["notehead"] = gap_report::noteheadJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::ExpressionClassification>) {
                result["type"] = "expression";
                result["expression"] = gap_report::expressionJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::FormattedText>) {
                result["type"] = "formatted-text";
                result["text"] = gap_report::formattedTextJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::PlaybackOnly>) {
                result["type"] = "playback-only";
            } else if constexpr (std::is_same_v<Payload, classify::SmartShapeClassification>) {
                result["type"] = "smart-shape";
                result["smartShape"] = gap_report::smartShapeJson(payload, arrowheads);
            } else if constexpr (std::is_same_v<Payload, classify::LyricWordExtension>) {
                result["type"] = "lyric-word-extension";
                result["wordExtension"] = gap_report::lyricWordExtensionJson(payload);
            }
        },
        gap.payload);
    return result;
}

} // namespace

std::string serializeGapReport(const classify::GapCollector& collector, const GapReportOptions& options)
{
    gap_report::ArrowheadTable arrowheads(options.glyphMetrics);
    auto gaps = json::array();
    for (const auto& gap : collector.gaps()) {
        gaps.push_back(gapJson(gap, arrowheads));
    }
    const auto& producer = options.producer;
    json result{
        {"schemaVersion", 1},
        {"producer", {{"name", producer.name}, {"version", producer.version}, {"commit", producer.commit}}},
        {"gaps", std::move(gaps)},
    };
    if (auto arrowheadJson = arrowheads.toJson(); !arrowheadJson.empty()) {
        result["arrowheads"] = std::move(arrowheadJson);
    }
    return result.dump(2);
}

} // namespace denigma
