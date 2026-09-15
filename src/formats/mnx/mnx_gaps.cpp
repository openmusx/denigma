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
#include "mnx_gaps.h"

#include <algorithm>

#include "denigma/classify/chords.h"
#include "denigma/gaps.h"
#include "mnx.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

void reportChordSymbolGaps(const std::shared_ptr<MnxMusxMapping>& context, std::string_view measureId, std::optional<int> staff,
    const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffId)
{
    if (!context->denigmaContext->gapCollector) {
        return;
    }
    const auto assignments =
        context->document->getDetails()->getArray<details::ChordAssign>(musxMeasure->getRequestedPartId(), staffId, musxMeasure->getCmper());
    const auto keySignature = musxMeasure->createKeySignature(staffId);
    for (const auto& assignment : assignments) {
        const auto classification = classify::classifyChordSymbol(assignment, keySignature, KeySignature::KeyContext::Written);
        if (!classification) {
            context->logMessage(LogMsg() << "could not classify chord symbol in measure " << musxMeasure->getCmper() << ", staff " << staffId << ".",
                MessageSeverity::Warning);
            continue;
        }
        const auto position = Fraction::fromEdu((std::max)(Edu{}, assignment->horzEdu));
        context->denigmaContext->gapCollector->add(
            {std::string(measureId), staff, GapPosition{position.numerator(), position.denominator()}}, *classification);
    }
}

void reportNoteheadGap(const std::shared_ptr<MnxMusxMapping>& context, std::string_view noteId,
    const classify::NoteheadClassification& classification, NoteType noteType)
{
    if (!context->denigmaContext->gapCollector || !classification.calcOverridesDefault(noteType)) {
        return;
    }
    context->denigmaContext->gapCollector->add({std::string(noteId), std::nullopt, std::nullopt}, classification);
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
