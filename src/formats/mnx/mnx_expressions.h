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
#pragma once

#include "core/denigma.h"
#include "mnxdom.h"
#include "musx/musx.h"

#include "mnx_fwd.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

/// @brief Exports the expressions of one measure whose types belong to the score as a whole rather than to a staff.
///
/// Tempo marks become the global measure's tempos (see design-decisions.md). A global type MNX has
/// no object for is reported as a conversion gap anchored to the global measure when a gap collector
/// is present. A marking a staff list draws on several staves is handled once, as one marking.
/// Every other expression type is a staff expression and is left to #processExpressions.
void processGlobalExpressions(
    const MnxMusxMappingPtr& context, mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure);

/// @brief Exports the expressions of one measure that belong to one staff of a part.
///
/// An expression MNX cannot represent on a staff is reported as a conversion gap anchored to the part
/// measure when a gap collector is present.
void processExpressions(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
