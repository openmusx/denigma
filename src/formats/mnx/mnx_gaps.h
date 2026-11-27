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

#include <optional>
#include <string>

#include "core/denigma.h"
#include "denigma/classify/gaps.h"
#include "musx/musx.h"

#include "mnx_fwd.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

/// @brief The gap collector, or null when no report was requested.
classify::GapCollector* gapCollectorFor(const MnxMusxMappingPtr& context);

/// @brief An anchor within a part measure already known by its id.
classify::GapAnchor measureAnchor(const std::string& measureId, std::optional<int> mnxStaffNumber, Edu eduPosition);

/// @brief An anchor for a place on a Finale staff, in target ids: the part measure, the staff number when
/// the part has several staves, and the position when one is given.
/// @return std::nullopt when the staff belongs to no exported instrument.
std::optional<classify::GapAnchor> partMeasureAnchor(
    const MnxMusxMappingPtr& context, StaffCmper staff, MeasCmper measure, std::optional<Fraction> position);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
