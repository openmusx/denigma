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

#include <optional>
#include <string>

#include "denigma/classify/gaps.h"
#include "denigma/classify/noteheads.h"
#include "mnx.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {

constexpr std::string_view NULL_NOTEHEAD_GLYPH = "noteheadNull";

/// Returns the glyph name of @p classification. A blank character has none, so it is named as the null notehead.
std::optional<std::string> calcGlyphName(const classify::NoteheadClassification& classification)
{
    if (classification.glyphName) {
        return classification.glyphName;
    }
    if (classification.shape == classify::notehead::Shape::Null) {
        return std::string(NULL_NOTEHEAD_GLYPH);
    }
    return std::nullopt;
}

} // namespace

classify::notehead::Shape calcKitComponentNoteheadFamily(const classify::PercussionNoteheadClassification& percussionNoteheads)
{
    return percussionNoteheads ? percussionNoteheads.family : classify::notehead::Shape::Regular;
}

void processKitComponentNotehead(mnxdom::part::KitComponent& kitComponent, const classify::PercussionNoteheadClassification& percussionNoteheads)
{
    const auto family = calcKitComponentNoteheadFamily(percussionNoteheads);
    if (family == classify::notehead::Shape::Regular) {
        return;
    }
    const auto mnxShape = enumConvert<mnxdom::NoteheadShape>(family);
    auto mnxNotehead = kitComponent.ensure_notehead(mnxShape);
    // A component's glyph would replace the notehead at every duration, so it is written only where MNX
    // requires one to name the shape at all.
    if (mnxShape == mnxdom::NoteheadShape::Custom) {
        if (const auto glyphName = calcGlyphName(percussionNoteheads.quarterAndShorter)) {
            mnxNotehead.set_glyph(*glyphName);
        }
    }
}

void processNotehead(const MnxMusxMappingPtr& context, mnxdom::sequence::NoteBase& mnxNote, std::string_view noteId, const NoteInfoPtr& musxNote,
    classify::notehead::Shape inheritedFamily)
{
    using Shape = classify::notehead::Shape;
    using Source = NoteInfoPtr::NoteheadInfo::Source;
    const auto noteType = std::get<0>(musxNote.getEntryInfo()->getEntry()->calcDurationInfo());
    auto classification = classify::classifyNotehead(musxNote);
    // An unclassified notehead names no glyph, so MNX cannot carry it. Only a Finale notehead override is
    // known to differ from the default: a percussion map or shape-note convention whose glyph cannot be
    // named is most likely the regular notehead in its font.
    if (!classification) {
        if (classification.noteheadInfo.source == Source::NoteAlteration) {
            if (auto* const gapCollector = context->denigmaContext->gapCollector) {
                gapCollector->add({std::string(noteId), std::nullopt, std::nullopt}, std::move(classification));
            }
        }
        return;
    }
    // A Finale notehead override is the author's choice for this note, so it is written even where it
    // matches what the note would inherit.
    if (classification.noteheadInfo.source != Source::NoteAlteration && !classification.calcDiffersFromFamily(inheritedFamily, noteType)) {
        return;
    }
    const auto mnxShape = enumConvert<mnxdom::NoteheadShape>(classification.shape);
    auto mnxNotehead = mnxNote.ensure_notehead(mnxShape);
    // MNX derives the glyph from the shape and the note value. Name the glyph where that derivation cannot
    // reproduce it: a custom shape, a fill that differs from the note value's, or a slash size MNX does not
    // distinguish.
    const bool needsGlyph = mnxShape == mnxdom::NoteheadShape::Custom || classification.calcFillOverridesDefault(noteType)
                            || classification.shape == Shape::SmallSlash || classification.shape == Shape::LargeSlash;
    if (needsGlyph) {
        if (const auto glyphName = calcGlyphName(classification)) {
            mnxNotehead.set_glyph(*glyphName);
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
