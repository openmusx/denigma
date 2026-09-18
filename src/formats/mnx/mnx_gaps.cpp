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

#include "mnx_gaps.h"

#include <algorithm>

#include "core/element_ids.h"
#include "mnx.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

classify::GapCollector* gapCollectorFor(const MnxMusxMappingPtr& context)
{
    return context->denigmaContext->gapCollector;
}

classify::GapAnchor measureAnchor(const std::string& measureId, std::optional<int> mnxStaffNumber, Edu eduPosition)
{
    return {measureId, mnxStaffNumber, Fraction::fromEdu(eduPosition)};
}

std::optional<classify::GapAnchor> partMeasureAnchor(
    const MnxMusxMappingPtr& context, StaffCmper staff, MeasCmper measure, std::optional<Fraction> position)
{
    const auto partIt = context->inst2Part.find(staff);
    if (partIt == context->inst2Part.end()) {
        context->logMessage(LogMsg() << "Staff " << staff << " is not part of any exported instrument, so the gap report cannot place a feature "
                                     << "in its measure " << measure << ".",
            MessageSeverity::Verbose);
        return std::nullopt;
    }
    const auto& staves = context->part2Inst.at(partIt->second);
    const auto staffIt = std::find(staves.begin(), staves.end(), staff);
    const auto staffNumber = (staves.size() > 1 && staffIt != staves.end()) ? std::optional<int>(int(staffIt - staves.begin()) + 1) : std::nullopt;
    return classify::GapAnchor{core::calcPartMeasureId(partIt->second, measure), staffNumber, position};
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
