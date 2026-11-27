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

MnxNotehead calcKitComponentNotehead(const classify::PercussionNoteheadClassification& percussionNoteheads)
{
    if (!percussionNoteheads || percussionNoteheads.family == classify::notehead::Shape::Regular) {
        return {};
    }
    MnxNotehead result{enumConvert<mnxdom::NoteheadShape>(percussionNoteheads.family), std::nullopt};
    // A component's glyph would replace the notehead at every duration, so it is written only where MNX
    // requires one to name the shape at all.
    if (result.shape == mnxdom::NoteheadShape::Custom) {
        result.glyph = calcGlyphName(percussionNoteheads.quarterAndShorter);
    }
    return result;
}

MnxNotehead calcNoteNotehead(const classify::NoteheadClassification& classification, musx::dom::NoteType noteType)
{
    MnxNotehead result{enumConvert<mnxdom::NoteheadShape>(classification.shape), std::nullopt};
    // MNX derives the glyph from the shape and the note value. Name the glyph where that derivation cannot
    // reproduce it: a custom shape, a fill that differs from the note value's, or a glyph the classifier
    // prefers to be named because it is not the standard glyph of its shape.
    if (result.shape == mnxdom::NoteheadShape::Custom || classification.calcFillOverridesDefault(noteType) || classification.glyphPreferred) {
        result.glyph = calcGlyphName(classification);
    }
    return result;
}

void processKitComponentNotehead(mnxdom::part::KitComponent& kitComponent, const classify::PercussionNoteheadClassification& percussionNoteheads)
{
    const auto notehead = calcKitComponentNotehead(percussionNoteheads);
    if (notehead == MnxNotehead{}) {
        return;
    }
    auto mnxNotehead = kitComponent.ensure_notehead(notehead.shape);
    if (notehead.glyph) {
        mnxNotehead.set_glyph(*notehead.glyph);
    }
}

void processNotehead(const MnxMusxMappingPtr& context, mnxdom::sequence::NoteBase& mnxNote, std::string_view noteId, const NoteInfoPtr& musxNote,
    const MnxNotehead& inherited)
{
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
    const auto notehead = calcNoteNotehead(classification, noteType);
    // A Finale notehead override is the author's choice for this note, so it is written even where it
    // matches what the note would inherit. Otherwise the note writes a notehead only where MNX would
    // draw something different without one.
    if (classification.noteheadInfo.source != Source::NoteAlteration && notehead == inherited) {
        return;
    }
    auto mnxNotehead = mnxNote.ensure_notehead(notehead.shape);
    if (notehead.glyph) {
        mnxNotehead.set_glyph(*notehead.glyph);
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
