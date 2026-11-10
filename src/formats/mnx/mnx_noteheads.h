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
#include "denigma/classify/noteheads.h"
#include "mnxdom.h"
#include "musx/musx.h"

#include "mnx_fwd.h"

using namespace musx::dom;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

/// @brief Returns the notehead family a kit note inherits from the kit component of @p percussionNoteheads.
///
/// This is the regular family when #processKitComponentNotehead writes nothing for the component.
classify::notehead::Shape calcKitComponentNoteheadFamily(const classify::PercussionNoteheadClassification& percussionNoteheads);

/// @brief Writes the default notehead of @p kitComponent, whose percussion note has the noteheads @p percussionNoteheads.
///
/// Nothing is written when the percussion note's notehead family is regular or unrecognized.
void processKitComponentNotehead(mnxdom::part::KitComponent& kitComponent, const classify::PercussionNoteheadClassification& percussionNoteheads);

/// @brief Writes the notehead of @p mnxNote, whose musx counterpart is @p musxNote.
///
/// A notehead is written when the Finale note has its own notehead override, or when its notehead differs
/// from @p inheritedFamily, the family a note inherits without one. A Finale notehead override that cannot be
/// classified is reported as a conversion gap anchored to @p noteId when a gap collector is present.
void processNotehead(const MnxMusxMappingPtr& context, mnxdom::sequence::NoteBase& mnxNote, std::string_view noteId, const NoteInfoPtr& musxNote,
    classify::notehead::Shape inheritedFamily);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
