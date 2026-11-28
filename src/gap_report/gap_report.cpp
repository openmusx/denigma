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
#include <vector>

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

/// The kind a placement names: the system edge a floating member draws on, or a staff.
std::string_view placementKindName(classify::ExpressionScope scope)
{
    switch (scope) {
    case classify::ExpressionScope::TopStaff: return "system-top";
    case classify::ExpressionScope::BottomStaff: return "system-bottom";
    case classify::ExpressionScope::Staff: return "staff";
    case classify::ExpressionScope::Unassigned: break;
    }
    return "unknown";
}

std::string_view verticalPlacementName(musx::dom::VerticalPlacement placement)
{
    using Enum = musx::dom::VerticalPlacement;
    switch (placement) {
    case Enum::NotApplicable: return "not-applicable";
    case Enum::Float: return "float";
    case Enum::Below: return "below";
    case Enum::Above: return "above";
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

json textRangesJson(const std::vector<classify::GapTextRange>& ranges)
{
    auto result = json::array();
    for (const auto& range : ranges) {
        result.push_back({{"anchor", range.id}, {"offset", range.offset}, {"length", range.length}});
    }
    return result;
}

json expressionPlacementsJson(const classify::ExpressionGap& gap)
{
    auto result = json::array();
    for (const auto& placement : gap.placements) {
        const auto& member = gap.classification.members.at(placement.member);
        json item{{"kind", placementKindName(member.scope)}};
        item.update(anchorJson(placement.anchor));
        if (member.placement != musx::dom::VerticalPlacement::NotApplicable) {
            item["placement"] = verticalPlacementName(member.placement);
        }
        result.push_back(std::move(item));
    }
    return result;
}

json pageTextPlacementsJson(const classify::PageTextGap& gap)
{
    auto result = json::array();
    for (const auto& page : gap.pages) {
        const auto& placement = gap.classification.placements.at(page.placement);
        json item{
            {"kind", "page"},
            {"anchor", page.pageId},
        };
        if (!placement.assignment->isPlacedLike(*gap.classification.assignment)) {
            item["position"] = gap_report::pageTextPositionJson(*placement.assignment);
        }
        result.push_back(std::move(item));
    }
    return result;
}

json gapJson(const classify::Gap& gap, gap_report::ReportTables& tables)
{
    json result = gap.anchor ? anchorJson(*gap.anchor) : json::object();
    result["extent"] = gapExtentName(gap.extent);
    std::visit(
        [&](const auto& payload) {
            using Payload = std::decay_t<decltype(payload)>;
            if constexpr (std::is_same_v<Payload, classify::ChordSymbolClassification>) {
                result["type"] = "chord-symbol";
                result["chord"] = gap_report::chordJson(payload, tables.fretboards);
            } else if constexpr (std::is_same_v<Payload, classify::NoteheadClassification>) {
                result["type"] = "notehead";
                result["notehead"] = gap_report::noteheadJson(payload, tables.fonts);
            } else if constexpr (std::is_same_v<Payload, classify::ExpressionGap>) {
                result["type"] = "expression";
                result["expression"] = gap_report::expressionJson(payload.classification, tables.fonts);
                if (!payload.placements.empty()) {
                    result["expression"]["placements"] = expressionPlacementsJson(payload);
                }
            } else if constexpr (std::is_same_v<Payload, classify::FormattedText>) {
                result["type"] = "formatted-text";
                result["text"] = gap_report::formattedTextJson(payload, tables.fonts);
            } else if constexpr (std::is_same_v<Payload, classify::TextFormatting>) {
                result["type"] = "text-formatting";
                result["formatting"] = {
                    {"font", tables.fonts.reference(payload.font)},
                    {"baseline", payload.baseline},
                    {"superscript", payload.superscript},
                    {"tracking", payload.tracking},
                    {"sourceText", payload.sourceText},
                    {"ranges", textRangesJson(payload.ranges)},
                };
            } else if constexpr (std::is_same_v<Payload, classify::TextInsert>) {
                result["type"] = "text-insert";
                result["insert"] = gap_report::insertJson(payload.insert);
                result["insert"]["sourceText"] = payload.sourceText;
                result["insert"]["ranges"] = textRangesJson(payload.ranges);
            } else if constexpr (std::is_same_v<Payload, classify::PlaybackOnly>) {
                result["type"] = "playback-only";
            } else if constexpr (std::is_same_v<Payload, classify::SmartShapeGap>) {
                result["type"] = "smart-shape";
                result["smartShape"] = gap_report::smartShapeJson(payload.classification, tables);
                if (payload.end) {
                    result["smartShape"]["end"] = anchorJson(*payload.end);
                }
            } else if constexpr (std::is_same_v<Payload, classify::MeasureTextClassification>) {
                result["type"] = "text-block";
                result["textBlock"] = gap_report::measureTextJson(payload);
            } else if constexpr (std::is_same_v<Payload, classify::PageTextGap>) {
                result["type"] = "page-text";
                result["pageText"] = gap_report::pageTextJson(payload.classification, tables.fonts);
                if (!payload.pages.empty()) {
                    result["pageText"]["placements"] = pageTextPlacementsJson(payload);
                }
            }
        },
        gap.payload);
    return result;
}

} // namespace

std::string serializeGapReport(const classify::GapCollector& collector, const GapReportOptions& options)
{
    gap_report::ReportTables tables{gap_report::ArrowheadTable(options.glyphMetrics), {}, {}};
    auto gaps = json::array();
    // Page assignment ids depend on one document-wide value, which a consumer cannot recover from the
    // target document; it is written once, and only when it is not 0. See design-decisions.md.
    int leadingBlankPageSlots = 0;
    for (const auto& gap : collector.gaps()) {
        gaps.push_back(gapJson(gap, tables));
        if (const auto* pageText = std::get_if<classify::PageTextGap>(&gap.payload)) {
            leadingBlankPageSlots = pageText->classification.assignment->getDocument()->getMaxBlankPages();
        }
    }
    const auto& producer = options.producer;
    json result{
        {"schemaVersion", GAP_REPORT_SCHEMA_VERSION},
        {"producer", {{"name", producer.name}, {"version", producer.version}, {"commit", producer.commit}}},
        {"gaps", std::move(gaps)},
    };
    if (leadingBlankPageSlots != 0) {
        result["leadingBlankPageSlots"] = leadingBlankPageSlots;
    }
    if (auto fontJson = tables.fonts.toJson(); !fontJson.empty()) {
        result["fonts"] = std::move(fontJson);
    }
    if (auto arrowheadJson = tables.arrowheads.toJson(); !arrowheadJson.empty()) {
        result["arrowheads"] = std::move(arrowheadJson);
    }
    if (auto fretboardJson = tables.fretboards.toJson(); !fretboardJson.empty()) {
        result["fretboards"] = std::move(fretboardJson);
    }
    return result.dump(2);
}

} // namespace denigma
