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

#include <string>

#include "FormattedText.h"
#include "denigma/classify/gaps.h"
#include "mnx_fwd.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

/// Replaces the content of @p dst, which belongs to @p document, with @p src. Each chunk's id is assigned
/// through @p document, which maps it for lookup during export.
void setFormattedText(mnxdom::FormattedText dst, const classify::FormattedText& src, const std::string& textId, mnxdom::Document& document,
    classify::GapCollector* gaps = nullptr);

/// Returns @p src as formatted text. The text belongs to a document of its own, because MNX ids can only be
/// assigned through a document.

mnxdom::FormattedText makeFormattedText(
    const musx::util::EnigmaParsingContext& src, const std::string& textId, classify::GapCollector* gaps = nullptr);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
