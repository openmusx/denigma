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
#include <vector>

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

/// @brief Exports the measure-attached text blocks of one staff in one measure as positioned text.
///
/// The text's formatting and inserts, and the frame it is laid out in, are reported as partial gaps
/// anchored to the positioned text when a gap collector is present.
void processMeasureTexts(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber);

/// @brief Reports the page-attached text blocks of every linked part as gaps.
///
/// MNX has no page text, so each assignment is one complete gap, however many scores show it. The
/// gap has no anchor: it states its text, frame and page rule as the full score has them, and its
/// pages are the classifier's placements that this exporter wrote a page for. Nothing is recorded
/// without a gap collector.
/// @param pageCounts The number of pages exported for each score, in the order of the linked parts.
void processPageTexts(const MnxMusxMappingPtr& context, const std::vector<size_t>& pageCounts);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
