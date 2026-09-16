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

#include <string_view>

#include "core/denigma.h"
#include "musx/musx.h"

#include "mnx_fwd.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

/// @brief Processes the notehead of one note whose MNX counterpart has the id @p noteId.
///
/// MNX has no notehead-shape encoding yet, so a notehead that differs from the one implied by the note's
/// duration is reported as a conversion gap anchored to the note when a gap collector is present.
void processNotehead(const MnxMusxMappingPtr& context, std::string_view noteId, const NoteInfoPtr& musxNote);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
