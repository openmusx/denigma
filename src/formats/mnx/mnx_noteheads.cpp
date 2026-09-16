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
#include "mnx_noteheads.h"

#include <string>

#include "denigma/classify/gaps.h"
#include "denigma/classify/noteheads.h"
#include "mnx.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

void processNotehead(const MnxMusxMappingPtr& context, std::string_view noteId, const NoteInfoPtr& musxNote)
{
    auto* const gapCollector = context->denigmaContext->gapCollector;
    if (!gapCollector) {
        return;
    }
    const auto noteType = std::get<0>(musxNote.getEntryInfo()->getEntry()->calcDurationInfo());
    auto classification = classify::classifyNotehead(musxNote);
    if (!classification.calcOverridesDefault(noteType)) {
        return;
    }
    gapCollector->add({std::string(noteId), std::nullopt, std::nullopt}, std::move(classification));
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
