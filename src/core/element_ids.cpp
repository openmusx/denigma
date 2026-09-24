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

#include "core/element_ids.h"

#include "core/denigma.h"

namespace denigma {
namespace core {

std::string calcEventId(const musx::dom::EntryInfoPtr& entryInfo)
{
    /// @todo Mirror entries from pre-Finale 25 documents can occur in multiple frames; prepend the frame number as `fr<cmper>` to keep their ids unique.
    return "ev" + std::to_string(entryInfo->getEntry()->getEntryNumber());
}

std::string calcNoteId(const musx::dom::NoteInfoPtr& noteInfo)
{
    return calcEventId(noteInfo.getEntryInfo()) + "n" + std::to_string(noteInfo->getNoteId());
}

std::string calcGlobalMeasureId(musx::dom::Cmper cmperValue)
{
    return "m" + std::to_string(cmperValue);
}

std::string calcPartMeasureId(const std::string& partId, musx::dom::Cmper cmperValue)
{
    return partId + "." + calcGlobalMeasureId(cmperValue);
}

std::string calcExpressionId(const musx::dom::MusxInstance<musx::dom::others::MeasureExprAssign>& assignment)
{
    ASSERT_IF (!assignment) {
        return {};
    }
    const auto inci = ".inci" + std::to_string(assignment->getInci().value_or(0));
    if (assignment->textExprId) {
        return calcGlobalMeasureId(assignment->getCmper()) + ".textExp" + std::to_string(assignment->textExprId) + inci;
    }
    if (assignment->shapeExprId) {
        return calcGlobalMeasureId(assignment->getCmper()) + ".shapeExp" + std::to_string(assignment->shapeExprId) + inci;
    }
    return {};
}

std::string calcTempoDefId(musx::dom::Cmper measureCmper, musx::dom::Inci inci)
{
    return calcGlobalMeasureId(measureCmper) + ".tempoDef.inci" + std::to_string(inci);
}

std::string calcTupletId(const musx::dom::EntryInfoPtr& firstEntryInfo, const musx::dom::MusxInstance<musx::dom::details::TupletDef>& tupletDef)
{
    return calcEventId(firstEntryInfo) + ".inci" + std::to_string(tupletDef->getInci().value_or(0));
}

} // namespace core
} // namespace denigma
