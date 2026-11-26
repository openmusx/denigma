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

/// @struct MnxNotehead
/// @brief A notehead as MNX writes it: a shape and, where one is named, a glyph.
struct MnxNotehead
{
    mnxdom::NoteheadShape shape{mnxdom::NoteheadShape::Oval}; ///< The MNX notehead shape.
    std::optional<std::string> glyph;                         ///< The SMuFL glyph name, when one is written.

    bool operator==(const MnxNotehead&) const = default;
};

/// @brief Returns the notehead a kit note inherits from the kit component of @p percussionNoteheads.
///
/// This is the reader's default oval notehead when #processKitComponentNotehead writes nothing for the component.
MnxNotehead calcKitComponentNotehead(const classify::PercussionNoteheadClassification& percussionNoteheads);

/// @brief Returns the notehead MNX writes for a note of @p noteType whose notehead is @p classification.
/// @pre @p classification is classified.
MnxNotehead calcNoteNotehead(const classify::NoteheadClassification& classification, musx::dom::NoteType noteType);

/// @brief Writes the default notehead of @p kitComponent, whose percussion note has the noteheads @p percussionNoteheads.
///
/// Nothing is written when the percussion note's notehead family is regular or unrecognized.
void processKitComponentNotehead(mnxdom::part::KitComponent& kitComponent, const classify::PercussionNoteheadClassification& percussionNoteheads);

/// @brief Writes the notehead of @p mnxNote, whose musx counterpart is @p musxNote.
///
/// A notehead is written when the Finale note has its own notehead override, or when the notehead MNX would
/// write for it differs from @p inherited, the notehead the note draws without one. A Finale notehead override
/// that cannot be classified is reported as a conversion gap anchored to @p noteId when a gap collector is present.
void processNotehead(const MnxMusxMappingPtr& context, mnxdom::sequence::NoteBase& mnxNote, std::string_view noteId, const NoteInfoPtr& musxNote,
    const MnxNotehead& inherited);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
