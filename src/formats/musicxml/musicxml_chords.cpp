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

#include "musicxml.h"

#include <algorithm>
#include <string>
#include <utility>
#include <variant>

#include "denigma/classify/chords.h"
#include "utils/stringutils.h"

#include "mx/api/ChordData.h"
#include "mx/api/DirectionData.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace musicxml {
namespace detail {

namespace {

/// Exports the fretboard Finale displays with a chord: a structured diagram becomes the chord's frame,
/// and a font fretboard, which has no frame to export, is counted.
void assignFretboard(MusicXmlMusxMapping& context, mx::api::ChordData& chord, const classify::FretboardClassification& fretboardClassification)
{
    // A font fretboard resolves to one character and has no fret positions to export. The
    // assignment's own flags decide this; see the font-fretboard entry in design-decisions.md.
    if (std::holds_alternative<classify::FontFretboard>(fretboardClassification)) {
        context.fontFretboardChordCount++;
    }

    // calcFretboardDisplayData owns the whole visibility decision, so a diagram here means Finale displays
    // a structured fretboard for this chord and an empty result needs no further checks of our own.
    const auto* fretboard = std::get_if<FretboardDisplayData>(&fretboardClassification);
    if (fretboard) {
        chord.hasFrameData = true;
        chord.frameData.stringCount = fretboard->stringCount;
        chord.frameData.fretCount = fretboard->fretCount;
        switch (fretboard->unplayedStringDisplay) {
        case UnplayedStringDisplay::None: break;
        case UnplayedStringDisplay::Blank: chord.frameData.unplayed = ""; break;
        case UnplayedStringDisplay::Muted: chord.frameData.unplayed = "x"; break;
        }
        if (fretboard->showFretboardNumber && fretboard->fretboardNumber > 1) {
            chord.frameData.isFirstFretSpecified = true;
            chord.frameData.firstFret = fretboard->fretboardNumber;
            // The label is the fret number followed by the style's text, giving "4fr.". Finale
            // has no setting for the side; see the first-fret entry in design-decisions.md.
            if (fretboard->fretNumberText) {
                chord.frameData.firstFretText = std::to_string(fretboard->fretboardNumber) + *fretboard->fretNumberText;
                chord.frameData.firstFretLocation = mx::api::FirstFretLocation::right;
            }
        }

        for (const auto& cell : fretboard->cells) {
            // MusicXML marks an unplayed string with the frame's unplayed attribute, not a frame note.
            if (cell.shape == details::FretboardDiagram::Shape::Muted) {
                continue;
            }
            auto note = mx::api::FrameNoteData{};
            note.stringNumber = cell.string;
            note.fretNumber = cell.fret;
            if (cell.finger != 0) {
                note.fingering = cell.finger;
                note.isFingeringSpecified = true;
            }
            chord.frameData.notes.emplace_back(note);
        }

        const auto hasFrameNote = [&](int stringNumber) {
            return std::any_of(chord.frameData.notes.begin(), chord.frameData.notes.end(),
                [stringNumber](const auto& candidate) { return candidate.stringNumber == stringNumber; });
        };
        // A MusicXML barre lies on the fret of its two endpoint notes. Finale anchors a barre to its
        // endpoint strings whatever fret is played there, so an endpoint string stopped above the barre
        // gets a second frame note at the barre's fret to carry the marker; see the barre entry in
        // design-decisions.md.
        const auto markEndpoint = [&](int stringNumber, int barreFret, mx::api::FrameBarre marker) {
            auto& notes = chord.frameData.notes;
            const auto onString = [stringNumber](const auto& candidate) { return candidate.stringNumber == stringNumber; };
            const auto atBarre = std::find_if(
                notes.begin(), notes.end(), [&](const auto& candidate) { return onString(candidate) && candidate.fretNumber == barreFret; });
            if (atBarre != notes.end()) {
                atBarre->barre = marker;
                return;
            }
            const auto aboveBarre = std::find_if(
                notes.begin(), notes.end(), [&](const auto& candidate) { return onString(candidate) && candidate.fretNumber > barreFret; });
            if (aboveBarre != notes.end()) {
                auto note = mx::api::FrameNoteData{};
                note.stringNumber = stringNumber;
                note.fretNumber = barreFret;
                note.barre = marker;
                notes.emplace_back(note);
                return;
            }
            const auto anyNote = std::find_if(notes.begin(), notes.end(), onString);
            anyNote->barre = marker;
        };
        for (const auto& barre : fretboard->barres) {
            // MusicXML cannot express half a barre, so emit one only when both ends have a note.
            if (!hasFrameNote(barre.endString) || !hasFrameNote(barre.startString)) {
                continue;
            }
            // Frame notes are emitted from the highest string number down, so the barre starts on
            // Finale's end string (the highest) and stops on its start string (the lowest).
            markEndpoint(barre.endString, barre.fret, mx::api::FrameBarre::start);
            markEndpoint(barre.startString, barre.fret, mx::api::FrameBarre::stop);
        }
        std::stable_sort(chord.frameData.notes.begin(), chord.frameData.notes.end(),
            [](const auto& left, const auto& right) { return left.stringNumber > right.stringNumber; });
    }
}

} // namespace

void processChords(MusicXmlMusxMapping& context, mx::api::StaffData& staff, const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffId)
{
    const auto assignments = context.document->getDetails()->getArray<details::ChordAssign>(context.forPartId, staffId, musxMeasure->getCmper());
    if (assignments.empty()) {
        return;
    }

    const auto keySignature = musxMeasure->createKeySignature(staffId);
    if (!keySignature) {
        context.logMessage(
            LogMsg() << "Skipping chord symbols in measure " << musxMeasure->getCmper() << " because no effective key signature was found.",
            MessageSeverity::Warning);
        return;
    }

    const auto staffAtMeasure = others::StaffComposite::createCurrent(context.document, context.forPartId, staffId, musxMeasure->getCmper(), 0);
    const auto keyContext = staffAtMeasure && staffAtMeasure->calcUsesWrittenPitch(context.pitchMode) ? KeySignature::KeyContext::Written
                                                                                                      : KeySignature::KeyContext::Concert;
    for (const auto& [assignment, position, classification] : classify::classifyChordAssignments(assignments, keySignature, keyContext)) {
        if (!classification.showRoot) {
            continue;
        }

        auto chord = mx::api::ChordData{};
        chord.root = enumConvert<mx::api::Step>(classification.root.noteName);
        chord.rootAlter = classification.root.alteration;
        if (classification.rootLowerCase) {
            chord.rootText = std::string{utils::toLowerCase(music_theory::calcNoteNameLetter(classification.root.noteName))};
        }
        chord.text = "";
        const auto& suffix = classification.suffix;
        chord.chordKind = suffix.quality ? enumConvert<mx::api::ChordKind>(*suffix.quality) : mx::api::ChordKind::other;
        if (classification.showSuffix) {
            chord.text = suffix.calcText();
            for (const auto& degree : suffix.degrees) {
                auto extension = mx::api::Extension{};
                extension.extensionType = enumConvert<mx::api::ExtensionType>(degree.type);
                extension.setAlterValue(degree.alteration);
                // A degree the suffix text already spells out must not print a second time. Leave the
                // rest unspecified rather than writing "yes", as Finale's own export does.
                if (degree.impliedByText) {
                    extension.printObject = mx::api::Bool::no;
                }
                switch (degree.value) {
                case 1: extension.extensionNumber = mx::api::ExtensionNumber::first; break;
                case 2: extension.extensionNumber = mx::api::ExtensionNumber::second; break;
                case 3: extension.extensionNumber = mx::api::ExtensionNumber::third; break;
                case 4: extension.extensionNumber = mx::api::ExtensionNumber::fourth; break;
                case 5: extension.extensionNumber = mx::api::ExtensionNumber::fifth; break;
                case 6: extension.extensionNumber = mx::api::ExtensionNumber::sixth; break;
                case 7: extension.extensionNumber = mx::api::ExtensionNumber::seventh; break;
                case 9: extension.extensionNumber = mx::api::ExtensionNumber::ninth; break;
                case 11: extension.extensionNumber = mx::api::ExtensionNumber::eleventh; break;
                case 13: extension.extensionNumber = mx::api::ExtensionNumber::thirteenth; break;
                default: continue;
                }
                chord.extensions.emplace_back(extension);
            }
            // These describe how the exported degrees are laid out, so they mean nothing without any.
            // Leave them unspecified rather than writing "no", so that the layout is only constrained
            // where Finale's suffix calls for it. Finale's own export does the same.
            if (!chord.extensions.empty()) {
                if (suffix.parenthesizeDegrees) {
                    chord.parenthesesDegrees = mx::api::Bool::yes;
                }
                if (suffix.stackDegrees) {
                    chord.stackDegrees = mx::api::Bool::yes;
                }
            }
            if (suffix.hasUnrecognizedGlyphs) {
                context.logMessage(LogMsg() << "Chord suffix " << assignment->suffixId << " in measure " << musxMeasure->getCmper()
                                            << " contains unrecognized glyphs.",
                    MessageSeverity::Warning);
            }
        }
        if (classification.bass) {
            chord.bass = enumConvert<mx::api::Step>(classification.bass->noteName);
            chord.bassAlter = classification.bass->alteration;
            if (classification.bassLowerCase) {
                chord.bassText = std::string{utils::toLowerCase(music_theory::calcNoteNameLetter(classification.bass->noteName))};
            }
            if (classification.bassArrangement) {
                chord.bassArrangement = enumConvert<mx::api::BassArrangement>(*classification.bassArrangement);
            }
        }

        if (classification.fretboard) {
            assignFretboard(context, chord, *classification.fretboard);
        }

        auto direction = mx::api::DirectionData{};
        direction.tickTimePosition = context.timing.calcNearestMusicXmlDivisions(position);
        direction.chords.emplace_back(std::move(chord));
        staff.directions.emplace_back(std::move(direction));
    }
}

} // namespace detail
} // namespace musicxml
} // namespace formats
} // namespace denigma
