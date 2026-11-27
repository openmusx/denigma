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

#include "denigma/classify/text_blocks.h"

#include <algorithm>
#include <map>
#include <utility>

namespace denigma {
namespace classify {

using namespace musx::dom;

TextBlockFrame classifyTextBlockFrame(const others::TextBlock& textBlock)
{
    TextBlockFrame result;
    result.width = textBlock.width;
    result.height = textBlock.height;
    result.handleOffsetX = textBlock.xAdd;
    result.handleOffsetY = textBlock.yAdd;
    result.wordWrap = textBlock.wordWrap;
    result.noExpandSingleWord = textBlock.noExpandSingleWord;
    result.justify = textBlock.justify;
    result.inset = EvpuFloat(textBlock.inset) / EFIX_PER_EVPU;
    result.lineSpacingPercent = textBlock.lineSpacingPercentage;
    result.lineSpacing = textBlock.lineSpacingEvpu;
    result.showBorder = textBlock.showShape;
    result.borderThickness = EvpuFloat(textBlock.stdLineThickness) / EFIX_PER_EVPU;
    result.cornerRadius = textBlock.roundCorners ? EvpuFloat(textBlock.cornerRadius) / EFIX_PER_EVPU : EvpuFloat{};
    result.positionFromFrameEdge = textBlock.newPos36;
    result.hasCustomShape = textBlock.shapeId != 0;
    return result;
}

namespace {

std::optional<MeasureTextClassification> classifyMeasureTextAssignment(const MusxInstance<details::MeasureTextAssign>& assignment, Cmper forPartId)
{
    const auto textBlock = assignment->getTextBlock();
    if (!textBlock) {
        return std::nullopt;
    }
    const auto staff = others::StaffComposite::createCurrent(assignment->getDocument(), forPartId, StaffCmper(assignment->getCmper1()),
        MeasCmper(assignment->getCmper2()), (std::max)(Edu{}, assignment->xDispEdu));
    if (!staff) {
        return std::nullopt;
    }
    MeasureTextClassification result;
    result.assignment = assignment;
    if (const auto text = assignment->getRawTextCtx(forPartId)) {
        result.text = classifyFormattedText(text);
    }
    result.frame = classifyTextBlockFrame(*textBlock);
    if (assignment->xDispEdu > 0) {
        result.position = musx::util::Fraction::fromEdu(assignment->xDispEdu);
    } else {
        result.horizontalOffset = assignment->xDispEvpu;
    }
    result.verticalOffset = assignment->yDisp - staff->calcTopLineEvpu();
    const Evpu frameY = assignment->yDisp + textBlock->yAdd;
    if (frameY > staff->calcTopLineEvpu()) {
        result.placement = VerticalPlacement::Above;
    } else if (frameY < staff->calcBottomLineEvpu()) {
        result.placement = VerticalPlacement::Below;
    }
    return result;
}

} // namespace

std::vector<MeasureTextClassification> classifyMeasureTexts(const MusxInstance<others::Measure>& measure, StaffCmper staffId)
{
    std::vector<MeasureTextClassification> result;
    if (!measure->hasTextBlock) {
        return result;
    }
    const auto partId = measure->getRequestedPartId();
    for (const auto& assignment : measure->getDocument()->getDetails()->getArray<details::MeasureTextAssign>(partId, staffId, measure->getCmper())) {
        if (assignment->hidden) {
            continue;
        }
        if (auto classification = classifyMeasureTextAssignment(assignment, partId)) {
            result.push_back(std::move(*classification));
        }
    }
    return result;
}

std::vector<PageTextClassification> classifyPageTexts(const DocumentPtr& document, const std::vector<Cmper>& partIds)
{
    std::vector<PageTextClassification> result;
    std::map<std::pair<Cmper, Inci>, size_t> indexByAssignment;
    for (const Cmper partId : partIds) {
        for (const auto& assignment : document->getOthers()->getArray<others::PageTextAssign>(partId)) {
            if (assignment->hidden) {
                continue;
            }
            if (!assignment->getTextBlock() || !assignment->calcStartPageNumber(partId)) {
                continue;
            }
            const auto cmper = assignment->getCmper();
            const auto inci = assignment->getInci().value_or(0);
            const auto [it, inserted] = indexByAssignment.emplace(std::pair{cmper, inci}, result.size());
            if (inserted) {
                // The text and frame are the full score's, classified once.
                auto base = assignment;
                Cmper basePartId = partId;
                if (partId != SCORE_PARTID) {
                    if (auto scoreAssignment = document->getOthers()->get<others::PageTextAssign>(SCORE_PARTID, cmper, inci)) {
                        base = std::move(scoreAssignment);
                        basePartId = SCORE_PARTID;
                    }
                }
                PageTextClassification classification;
                classification.assignment = base;
                if (const auto text = base->getRawTextCtx(basePartId)) {
                    classification.text = classifyFormattedText(text);
                }
                if (const auto textBlock = base->getTextBlock()) {
                    classification.frame = classifyTextBlockFrame(*textBlock);
                }
                result.push_back(std::move(classification));
            }
            auto& placements = result[it->second].placements;
            // The pages are limited to the part's page list and filtered by the odd/even setting.
            for (const PageCmper page : assignment->calcPageNumbers(partId)) {
                placements.push_back({partId, page, assignment});
            }
        }
    }
    return result;
}

} // namespace classify
} // namespace denigma
