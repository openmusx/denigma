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
#include <string_view>

#include "gap_report_json.h"

namespace denigma {
namespace gap_report {

namespace {

using namespace classify;

std::string_view wordExtensionKindName(lyric::WordExtensionKind kind)
{
    switch (kind) {
    case lyric::WordExtensionKind::None: return "none";
    case lyric::WordExtensionKind::Smart: return "smart";
    case lyric::WordExtensionKind::Legacy: return "legacy";
    }
    return "none";
}

} // namespace

json lyricWordExtensionJson(const classify::LyricWordExtension& wordExtension)
{
    // A consumer learns from `reach` alone whether the extension has a known end, without inspecting
    // the gap's end anchor.
    return json{
        {"kind", wordExtensionKindName(wordExtension.kind)},
        {"reach", wordExtension.endEntry ? "event" : "unknown"},
    };
}

} // namespace gap_report
} // namespace denigma
