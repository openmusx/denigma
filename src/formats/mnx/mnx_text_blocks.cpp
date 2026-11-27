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

#include "mnx_text_blocks.h"

#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "core/element_ids.h"
#include "denigma/classify/gaps.h"
#include "denigma/classify/text_blocks.h"
#include "mnx.h"
#include "mnx_formatted_text.h"
#include "mnx_gaps.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

void processMeasureTexts(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber)
{
    auto* const gaps = gapCollectorFor(context);
    for (auto& classification : classify::classifyMeasureTexts(musxMeasure, context->current.staff)) {
        if (classification.text.runs.empty()) {
            continue;
        }
        const auto& assignment = classification.assignment;
        const auto id = core::calcMeasureTextId(assignment);
        auto positioned = mnxMeasure.ensure_text().append();
        context->mnxDocument->assignId(positioned, id);
        positioned.set_or_clear_staff(mnxStaffNumber.value_or(1));
        positioned.set_or_clear_placement(enumConvert<mnxdom::Placement>(classification.placement));
        if (classification.position != musx::util::Fraction{}) {
            positioned.ensure_position(mnxFractionFromFraction(classification.position));
        }
        setFormattedText(positioned.text(), classification.text, id, *context->mnxDocument, gaps);
        if (gaps) {
            gaps->add({id, std::nullopt, std::nullopt}, std::move(classification), classify::GapExtent::Partial);
        }
    }
}

void processPageTexts(const MnxMusxMappingPtr& context, const std::vector<size_t>& pageCounts)
{
    auto* const gaps = gapCollectorFor(context);
    if (!gaps) {
        return;
    }
    std::vector<Cmper> partIds;
    std::map<Cmper, size_t> pageCountByPart;
    for (size_t partIndex = 0; partIndex < context->musxParts.size() && partIndex < pageCounts.size(); ++partIndex) {
        const auto partId = context->musxParts[partIndex]->getCmper();
        partIds.push_back(partId);
        pageCountByPart.emplace(partId, pageCounts[partIndex]);
    }
    for (auto& classification : classify::classifyPageTexts(context->document, partIds)) {
        if (classification.text.runs.empty()) {
            continue;
        }
        classify::PageTextGap gap{std::move(classification), {}};
        const auto& placements = gap.classification.placements;
        for (size_t index = 0; index < placements.size(); ++index) {
            // A page beyond the exported ones (a part whose layout was never calculated exports none) has no id.
            const auto& placement = placements[index];
            if (placement.page > 0 && static_cast<size_t>(placement.page) <= pageCountByPart[placement.partId]) {
                gap.pages.push_back({core::calcPageId(placement.partId, placement.page), index});
            }
        }
        gaps->addUnanchored(std::move(gap));
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
