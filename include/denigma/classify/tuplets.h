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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include "musx/musx.h"

namespace denigma {
namespace classify {

/// @struct TupletClassification
/// @brief Shared display properties resolved from a Finale tuplet definition.
struct TupletClassification
{
    /// Vertical relationship of the tuplet to its notes.
    musx::dom::VerticalPlacement placement{musx::dom::VerticalPlacement::NotApplicable};
    /// Whether Finale's settings call for a bracket after resolving its automatic bracket rule.
    bool showBracket{};
    /// Whether Finale's settings call for a tuplet number or ratio.
    bool showNumber{};
};

/// Classifies shared tuplet display properties using the tuplet's complete entry span.
[[nodiscard]]
TupletClassification classifyTuplet(const musx::dom::EntryFrame::TupletInfo& tupletInfo, const musx::dom::EntryInfoPtr& firstEntryInfo);

} // namespace classify
} // namespace denigma
