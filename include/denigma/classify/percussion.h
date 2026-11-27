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

#include <compare>
#include <optional>
#include <string>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace percussion {

/// @struct SoundIdentity
/// @brief Identifies the instrument a percussion note plays.
///
/// The identity is the same in every percussion map, so notes that use different maps but the same
/// note type and copy play the same instrument.
struct SoundIdentity
{
    /// The note type, without its copy number. (See #musx::dom::others::PercussionNoteInfo::getBaseNoteTypeId.)
    musx::dom::PercussionNoteTypeId baseNoteTypeId{};
    /// Distinguishes copies of the same note type, starting at 0. (See #musx::dom::others::PercussionNoteInfo::getNoteTypeOrderId.)
    unsigned orderId{};

    /// Orders identities by note type, then by copy.
    auto operator<=>(const SoundIdentity&) const = default;
};

/// @struct MapNoteIdentity
/// @brief Identifies one percussion note within one percussion map.
///
/// The map owns the note's staff position and noteheads, so two maps can draw the same instrument differently.
struct MapNoteIdentity
{
    /// The percussion map. (See #musx::dom::others::Staff::percussionMapId.)
    musx::dom::Cmper percussionMapId{};
    /// The full note type, including its copy number.
    musx::dom::PercussionNoteTypeId noteTypeId{};

    /// Orders identities by map, then by note type.
    auto operator<=>(const MapNoteIdentity&) const = default;
};

} // namespace percussion

/// @struct PercussionNoteClassification
/// @brief Result returned by percussion note classification.
///
/// A note on a percussion staff is a percussion note whether or not the staff's percussion map assigns it.
/// One the map does not assign plays the MIDI key of its pitch.
struct PercussionNoteClassification
{
    /// True when the note is on a percussion staff, whether or not its percussion map assigns it.
    bool isOnPercussionStaff{};
    /// The percussion map's note. Null when the staff's percussion map does not assign the note.
    musx::dom::MusxInstance<musx::dom::others::PercussionNoteInfo> percussionNote;
    /// The percussion note within its map.
    percussion::MapNoteIdentity mapNote;
    /// The instrument the percussion note plays.
    percussion::SoundIdentity sound;
    /// The instrument's name, including its copy number when it has one. No value when Finale does not
    /// know the note type.
    std::optional<std::string> name;
    /// The instrument's General MIDI percussion key, counting from 0. No value when it has none.
    std::optional<int> generalMidiKey;
    /// The key the note's playback device plays for a mapped note, counting from 0, from the document's
    /// playback resources. (See #musx::dom::NoteInfoPtr::calcPercussionPlaybackKey.) When the device's note
    /// list has neither the note type nor its base type, this is the key of the nearest parent instrument it
    /// lists. No value when the lookup failed; #playbackLookupFailure says why.
    std::optional<int> playbackMidiKey;
    /// Why #playbackMidiKey has no value. No value when it has one, including from a parent instrument, or
    /// when the note is not mapped.
    std::optional<musx::dom::PlaybackLookupFailure> playbackLookupFailure;
    /// For a note on a percussion staff that its map does not assign, the MIDI key it plays, which is its
    /// concert pitch (C4 is 60). No value for a mapped note, or when the pitch is outside the MIDI range.
    std::optional<int> unmappedMidiKey;
    /// The note's staff position relative to the staff's reference line: the map's fixed position for a
    /// mapped note (see #musx::dom::others::PercussionNoteInfo::calcStaffReferencePosition), or the note's
    /// own position for an unmapped one.
    int staffReferencePosition{};

    /// Returns whether the note's playback device plays the note: its note list has the note type, its base
    /// type, or one of its parent instruments. A device that has none of them plays no kit piece for the note. Without a device, as when no playback resources were supplied,
    /// the note type's own General MIDI key stands in, so this is true.
    [[nodiscard]] bool isPlayedByDevice() const noexcept { return playbackLookupFailure != musx::dom::PlaybackLookupFailure::NoteTypeNotListed; }

    /// Returns the key the note plays, counting from 0: the playback device's key when it is known, and
    /// otherwise the note type's General MIDI key.
    [[nodiscard]] std::optional<int> calcMidiKey() const noexcept { return playbackMidiKey ? playbackMidiKey : generalMidiKey; }

    /// Returns true when the note is a mapped percussion note. (See #isOnPercussionStaff for any percussion note.)
    explicit operator bool() const noexcept { return percussionNote != nullptr; }
};

/// Classifies a percussion map's note.
/// @param percussionNote The percussion map's note. A null value returns an empty classification.
PercussionNoteClassification classifyPercussionNote(const musx::dom::MusxInstance<musx::dom::others::PercussionNoteInfo>& percussionNote);

/// Classifies the percussion note a note plays, resolved through its staff's effective percussion map.
/// (See #musx::dom::NoteInfoPtr::calcPercussionNoteInfo.) The classification is empty when the note
/// is not on a percussion staff.
/// @param note The note to classify.
/// @param pitchMode The pitch representation that places a note the map does not assign. A mapped note's
/// position is fixed by its map.
PercussionNoteClassification classifyPercussionNote(
    const musx::dom::NoteInfoPtr& note, musx::dom::PitchMode pitchMode = musx::dom::PitchMode::CurrentView);

} // namespace classify
} // namespace denigma
