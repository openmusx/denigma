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
#include "denigma/classify/percussion.h"

namespace denigma {
namespace classify {

namespace {

// A device that does not list a note type plays it as its parent instrument, and so on up the
// chain: a snare rim shot plays as the snare drum. Finale's exports show this for note types
// whose own entry a device's list leaves out.
std::optional<int> findParentInstrumentKey(const musx::dom::DeviceNoteList& noteList, musx::dom::PercussionNoteTypeId baseNoteType)
{
    // The table's chains are short. The limit only guards against a cycle.
    constexpr int maxChainLength = 16;
    auto noteType = baseNoteType;
    for (int step = 0; step < maxChainLength; ++step) {
        const auto parent =
            static_cast<musx::dom::PercussionNoteTypeId>(musx::dom::percussion::getPercussionNoteTypeFromId(noteType).parentInstrumentId);
        if (parent == noteType || parent == 0) {
            return std::nullopt;
        }
        if (const auto key = noteList.findMidiKey(parent, musx::dom::NoteTypeMatch::ExactThenBase)) {
            return key;
        }
        noteType = parent;
    }
    return std::nullopt;
}

} // namespace

PercussionNoteClassification classifyPercussionNote(const musx::dom::MusxInstance<musx::dom::others::PercussionNoteInfo>& percussionNote)
{
    if (!percussionNote) {
        return {};
    }
    PercussionNoteClassification result;
    result.isOnPercussionStaff = true;
    result.percussionNote = percussionNote;
    result.mapNote = {percussionNote->getCmper(), percussionNote->percNoteType};
    result.sound = {percussionNote->getBaseNoteTypeId(), percussionNote->getNoteTypeOrderId()};
    result.staffReferencePosition = percussionNote->calcStaffReferencePosition();
    const auto& noteType = percussionNote->getNoteType();
    // An instrument id of 0 is the note type table's placeholder for a note type it does not list.
    if (noteType.instrumentId != 0) {
        result.name = noteType.createName(result.sound.orderId);
    }
    if (noteType.generalMidi >= 0) {
        result.generalMidiKey = noteType.generalMidi;
    }
    return result;
}

PercussionNoteClassification classifyPercussionNote(const musx::dom::NoteInfoPtr& note, musx::dom::PitchMode pitchMode)
{
    if (auto result = classifyPercussionNote(note.calcPercussionNoteInfo())) {
        const auto playback = note.calcPercussionPlaybackKey();
        result.playbackMidiKey = playback.midiKey;
        result.playbackLookupFailure = playback.failure;
        if (playback.failure == musx::dom::PlaybackLookupFailure::NoteTypeNotListed && playback.noteList) {
            if (const auto parentKey = findParentInstrumentKey(*playback.noteList, result.sound.baseNoteTypeId)) {
                result.playbackMidiKey = parentKey;
                result.playbackLookupFailure.reset();
            }
        }
        return result;
    }
    const auto entryInfo = note.getEntryInfo();
    if (!entryInfo->getEntry()->isNote) {
        return {};
    }
    // The staff the frame already holds for the start of the measure decides, because instrument changes,
    // and with them percussion notation, are believed to occur only at barlines. It avoids creating a staff
    // for every note.
    const auto staff = entryInfo.getFrame()->getStartStaffInstance();
    if (!staff || !staff->percussionMapId) {
        return {};
    }
    PercussionNoteClassification result;
    result.isOnPercussionStaff = true;
    result.staffReferencePosition = note.calcNoteProperties({.pitchMode = pitchMode}).staffPosition;
    const auto concert = note.calcNoteProperties({.pitchMode = musx::dom::PitchMode::Concert});
    constexpr int maxMidiKey = 127;
    if (const int midiKey = music_theory::calcMidiKey({concert.noteName, concert.octave, concert.alteration});
        midiKey >= 0 && midiKey <= maxMidiKey) {
        result.unmappedMidiKey = midiKey;
    }
    return result;
}

} // namespace classify
} // namespace denigma
