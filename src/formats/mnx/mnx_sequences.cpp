/*
 * Copyright (C) 2024, Robert Patterson
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
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "core/element_ids.h"
#include "denigma/classify/lyrics.h"
#include "denigma/classify/tuplets.h"
#include "mnx.h"
#include "mnx_gaps.h"
#include "mnx_noteheads.h"
#include "mnx_smartshapes.h"
#include "utils/smufl_support.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

static void appendMeasureRemainderSpaces(
    mnxdom::sequence::SequenceContent content, const musx::util::Fraction& elapsedInVoice, const musx::util::Fraction& measureDuration)
{
    const auto remaining = measureDuration - elapsedInVoice;
    const int denom = remaining.denominator();
    ASSERT_IF (denom <= 0) {
        throw std::logic_error("Remaining duration has non-positive denominator.");
    }
    if (remaining <= 0 || (EDU_PER_WHOLE_NOTE % denom) != 0) {
        return;
    }

    const auto eduRemaining = remaining.calcEduDuration();

    std::vector<Edu> groups;
    int mask = 1;
    while (mask <= eduRemaining) {
        mask <<= 1;
    }
    mask >>= 1;

    bool inGroup = false;
    long long groupValue = 0;
    while (mask > 0) {
        if (eduRemaining & mask) {
            groupValue += mask;
            inGroup = true;
        } else if (inGroup) {
            groups.push_back(static_cast<Edu>(groupValue));
            groupValue = 0;
            inGroup = false;
        }
        mask >>= 1;
    }
    if (inGroup) {
        groups.push_back(static_cast<Edu>(groupValue));
    }

    for (auto it = groups.rbegin(); it != groups.rend(); ++it) {
        content.appendSpace(mnxFractionFromEdu(*it));
    }
}

static mnxdom::sequence::MultiNoteTremolo createMultiNoteTremolo(
    mnxdom::sequence::SequenceContent content, const musx::dom::EntryFrame::TupletInfo& tupletInfo, int marks)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    const auto entryCount = static_cast<unsigned>(tupletInfo.numEntries());
    const Edu eduRefDuration = (musxTuplet->calcReferenceDuration() / entryCount).calcEduDuration();
    auto mnxTremolo = content.appendMultiNoteTremolo(marks, mnxdom::NoteValueQuantity::make(entryCount, mnxNoteValueFromEdu(eduRefDuration)));
    /// @todo: additional fields (like noteheads) when defined by MNX committee.
    return mnxTremolo;
}

static mnxdom::sequence::Tuplet createTuplet(const MnxMusxMappingPtr& context, mnxdom::sequence::SequenceContent content,
    const musx::dom::EntryFrame::TupletInfo& tupletInfo, const EntryInfoPtr& firstEntryInfo)
{
    const auto& musxTuplet = tupletInfo.tuplet;
    const auto tupletClassification = classify::classifyTuplet(tupletInfo, firstEntryInfo);
    auto mnxTuplet = content.appendTuplet(
        mnxdom::NoteValueQuantity::make(static_cast<unsigned>(musxTuplet->displayNumber), mnxNoteValueFromEdu(musxTuplet->displayDuration)),
        mnxdom::NoteValueQuantity::make(static_cast<unsigned>(musxTuplet->referenceNumber), mnxNoteValueFromEdu(musxTuplet->referenceDuration)));

    mnxTuplet.set_or_clear_bracket(tupletClassification.showBracket ? mnxdom::AutoYesNo::Yes : mnxdom::AutoYesNo::No);

    mnxTuplet.set_or_clear_showNumber([&]() {
        if (!tupletClassification.showNumber) {
            return mnxdom::TupletDisplaySetting::NoNumber;
        }
        switch (musxTuplet->numStyle) {
        case details::TupletDef::NumberStyle::Number: return mnxdom::TupletDisplaySetting::Inner;
        case details::TupletDef::NumberStyle::Nothing: return mnxdom::TupletDisplaySetting::NoNumber;
        case details::TupletDef::NumberStyle::UseRatio: return mnxdom::TupletDisplaySetting::Both;
        case details::TupletDef::NumberStyle::RatioPlusDenominatorNote: return mnxdom::TupletDisplaySetting::Both;
        case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnxdom::TupletDisplaySetting::Both;
        }
        return mnxdom::TupletDisplaySetting::Inner;
    }());

    mnxTuplet.set_or_clear_showValue([&]() {
        if (!tupletClassification.showNumber) {
            return mnxdom::TupletDisplaySetting::NoNumber;
        }
        switch (musxTuplet->numStyle) {
        case details::TupletDef::NumberStyle::Number: return mnxdom::TupletDisplaySetting::NoNumber;
        case details::TupletDef::NumberStyle::Nothing: return mnxdom::TupletDisplaySetting::NoNumber;
        case details::TupletDef::NumberStyle::UseRatio: return mnxdom::TupletDisplaySetting::NoNumber;
        case details::TupletDef::NumberStyle::RatioPlusDenominatorNote:
            return mnxdom::TupletDisplaySetting::Inner; // should be Outer, but this is not currently an option
        case details::TupletDef::NumberStyle::RatioPlusBothNotes: return mnxdom::TupletDisplaySetting::Both;
        }
        return mnxdom::TupletDisplaySetting::NoNumber;
    }());

    mnxTuplet.set_or_clear_placement(enumConvert<mnxdom::Placement>(tupletClassification.placement));

    // A tuplet whose every entry crosses to the same staff is drawn on that staff.
    std::optional<StaffCmper> crossedStaff = firstEntryInfo.calcCrossedStaffForAll();
    for (auto entryInfo = firstEntryInfo; crossedStaff && entryInfo && entryInfo.getIndexInFrame() <= tupletInfo.endIndex;
        entryInfo = entryInfo.getNextSameV()) {
        if (entryInfo.calcCrossedStaffForAll() != crossedStaff) {
            crossedStaff.reset();
        }
    }
    if (crossedStaff) {
        if (const auto mnxPartStaff = context->mnxPartStaffFromStaff(crossedStaff.value())) {
            mnxTuplet.set_staff(mnxPartStaff.value());
        }
    }

    return mnxTuplet;
}

bool isOmittedZeroLengthTuplet(const EntryFrame::TupletInfo& tupletInfo)
{
    return tupletInfo.tuplet->calcRatio() == 0 && !tupletInfo.calcCreatesSingletonBeamLeft() && !tupletInfo.calcCreatesSingletonBeamRight();
}

bool isInOmittedZeroLengthTuplet(const EntryInfoPtr& entryInfo)
{
    for (size_t tupletIndex : entryInfo.findTupletInfo()) {
        if (isOmittedZeroLengthTuplet(entryInfo.getFrame()->tupletInfo[tupletIndex])) {
            return true;
        }
    }
    return false;
}

static void createTies(const MnxMusxMappingPtr& context, mnxdom::sequence::NoteBase& mnxNote, const NoteInfoPtr& musxNote)
{
    bool tieCreated = false;
    if (musxNote.calcHasTieStart()) {
        auto mnxTies = mnxNote.ensure_ties();
        auto tiedTo = musxNote.calcTieTo();
        auto mnxTie = mnxTies.append();
        if (tiedTo && tiedTo.calcHasTieEnd() && !tiedTo.getEntryInfo()->getEntry()->isHidden) {
            mnxTie.set_target(core::calcNoteId(tiedTo));
            context->deferredTieTargets.push_back({mnxTie.pointer(), tiedTo.getEntryInfo()->getEntry()->getEntryNumber()});
        } else {
            mnxTie.set_lv(true);
        }
        if (!mnxTie.lv()) {
            if (tiedTo.getEntryInfo().getVoice() == musxNote.getEntryInfo().getVoice()) {
                mnxTie.set_targetType(mnxdom::TieTargetType::NextNote);
            } else {
                mnxTie.set_targetType(mnxdom::TieTargetType::CrossVoice);
            }
        }
        if (auto tieAlter = context->document->getDetails()->getForNote<details::TieAlterStart>(musxNote)) {
            if (tieAlter->freezeDirection) {
                mnxTie.set_or_clear_side(tieAlter->down ? mnxdom::SlurTieSide::Down : mnxdom::SlurTieSide::Up);
            }
        }
        tieCreated = true;
    }
    using Curve = CurveContourDirection;
    if (const auto tiedToInfo = musxNote.calcArpeggiatedTieInfo()) {
        const NoteInfoPtr musxTargetNote(tiedToInfo->targetEntry, tiedToInfo->targetNoteIndex);
        auto mnxTies = mnxNote.ensure_ties();
        auto mnxTie = mnxTies.append();
        mnxTie.set_target(core::calcNoteId(musxTargetNote));
        mnxTie.set_targetType(mnxdom::TieTargetType::Arpeggio);
        context->deferredTieTargets.push_back({mnxTie.pointer(), tiedToInfo->targetEntry->getEntry()->getEntryNumber()});
        if (tiedToInfo->direction != Curve::Unspecified) {
            mnxTie.set_or_clear_side(tiedToInfo->direction == Curve::Up ? mnxdom::SlurTieSide::Up : mnxdom::SlurTieSide::Down);
        }
        tieCreated = true;
    }
    if (!tieCreated) {
        if (const auto pseudoTieInfo = musxNote.calcPseudoLvTieInfo()) {
            auto mnxTies = mnxNote.ensure_ties();
            auto mnxTie = mnxTies.append();
            mnxTie.set_lv(true);
            if (pseudoTieInfo.direction != Curve::Unspecified) {
                mnxTie.set_or_clear_side(pseudoTieInfo.direction == Curve::Up ? mnxdom::SlurTieSide::Up : mnxdom::SlurTieSide::Down);
            }
        }
    }
}

static void deferJumpTies(const MnxMusxMappingPtr& context, const NoteInfoPtr& musxNote)
{
    if (musxNote.getEntryInfo().getMeasure() == 4) {
        int x = 0;
        static_cast<void>(x);
    }
    if (musxNote.getEntryInfo()->getEntry()->isHidden) {
        return;
    }

    const auto jumpTies = musxNote.calcJumpTieContinuationsFrom();
    if (jumpTies.empty()) {
        return;
    }

    const auto endNoteId = core::calcNoteId(musxNote);
    for (const auto& [startNote, direction] : jumpTies) {
        if (!startNote || startNote.getEntryInfo()->getEntry()->isHidden) {
            continue;
        }
        const auto startNoteId = core::calcNoteId(startNote);
        const std::string key = startNoteId + "->" + endNoteId;
        if (!context->deferredJumpTieKeys.emplace(key).second) {
            continue;
        }

        MnxMusxMapping::DeferredJumpTie deferred{
            startNoteId,
            endNoteId,
        };
        if (direction != CurveContourDirection::Unspecified) {
            deferred.side = (direction == CurveContourDirection::Up) ? mnxdom::SlurTieSide::Up : mnxdom::SlurTieSide::Down;
        }
        context->deferredJumpTies.push_back(std::move(deferred));
    }
}

mnxdom::sequence::Note createNormalNote(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const NoteInfoPtr& musxNote)
{
    const auto properties = musxNote.calcNoteProperties({
        .pitchMode = PitchMode::Concert,
    });
    const int octave = properties.octave + calcOttavaOctaveAdjustment(context->current.ottavasApplicableInMeasure, musxNote, [&](const NoteInfoPtr&) {
        context->logMessage(
            LogMsg() << "skipping ottava octave setting for tied-to note since the tied-from note is not under the ottava", MessageSeverity::Verbose);
    });
    auto mnxNote = mnxEvent.ensure_notes().append(
        mnxdom::sequence::Pitch::make(enumConvert<mnxdom::NoteStep>(properties.noteName), octave, properties.alteration));
    if (musxNote->freezeAcci || musxNote->parenAcci) {
        auto acciDisp = mnxNote.ensure_accidentalDisplay(musxNote->showAcci);
        acciDisp.set_or_clear_force(musxNote->freezeAcci);
        if (musxNote->parenAcci) {
            acciDisp.ensure_enclosure(mnxdom::AccidentalEnclosureSymbol::Parentheses);
        }
    }
    const auto musxEntry = musxNote.getEntryInfo()->getEntry();
    if (musxNote.calcIsEnharmonicRespellInAnyPart()) {
        auto [enharmonicLev, enharmonicAlt] = musxNote.calcDefaultEnharmonic();
        auto mnxWritten = mnxNote.ensure_written();
        mnxWritten.set_diatonicDelta(enharmonicLev - musxNote->harmLev);
    }
    return mnxNote;
}

mnxdom::sequence::KitNote createKitNote(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent,
    const MusxInstance<others::PercussionNoteInfo>& percNoteInfo, const MusxInstance<others::Staff>& musxStaff)
{
    auto mnxNote = mnxEvent.ensure_kitNotes().append(calcPercussionKitId(percNoteInfo));
    auto part = mnxNote.getEnclosingElement<mnxdom::Part>();
    MNX_ASSERT_IF(!part.has_value())
    {
        throw std::logic_error("Note created without a part.");
    }
    part->ensure_kit();
    if (!part->kit()->contains(mnxNote.kitComponent())) {
        auto kitElement = part->kit()->append(mnxNote.kitComponent(), mnxStaffPosition(musxStaff, percNoteInfo->calcStaffReferencePosition()));
        const auto& percNoteType = percNoteInfo->getNoteType();
        if (percNoteType.instrumentId != 0) {
            kitElement.set_name(percNoteType.createName(percNoteInfo->getNoteTypeOrderId()));
            kitElement.set_sound(calcPercussionSoundId(percNoteInfo));
            auto sounds = context->mnxDocument->global().ensure_sounds();
            if (!sounds.contains(kitElement.sound().value())) {
                auto sound = sounds.append(kitElement.sound().value());
                sound.set_name(kitElement.name().value());
                if (percNoteType.generalMidi >= 0) {
                    /// @todo revisit this if there is better support for multiple synth patches
                    sound.set_midiNumber(percNoteType.generalMidi);
                }
            }
        }
    }
    return mnxNote;
}

template <typename MnxNoteType>
static void createNote(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const NoteInfoPtr& musxNote,
    const MusxInstance<others::Staff>& musxStaff, const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    static_assert(std::is_base_of_v<mnxdom::sequence::NoteBase, MnxNoteType>, "MnxNoteType must have base type NoteBase.");

    MnxNoteType mnxNote = [&]() {
        if constexpr (std::is_same_v<MnxNoteType, mnxdom::sequence::Note>) {
            return createNormalNote(context, mnxEvent, musxNote);
        } else {
            ASSERT_IF (!percNoteInfo) {
                throw std::logic_error("Kit note requested without PercussionNoteInfo instance.");
            }
            return createKitNote(context, mnxEvent, percNoteInfo, musxStaff);
        }
    }();
    const auto noteId = core::calcNoteId(musxNote);
    mnxNote.set_id(noteId);
    context->noteJsonById.emplace(noteId, mnxNote.pointer());
    processNotehead(context, noteId, musxNote);
    if (musxNote->crossStaff && !mnxEvent.staff()) { // createEvent already handled cross-staffing if the entire entry is crossed
        StaffCmper noteStaff = musxNote.calcStaff();
        if (const auto& mnxNoteStaff = context->mnxPartStaffFromStaff(noteStaff)) {
            mnxNote.set_staff(mnxNoteStaff.value());
        } else {
            context->logMessage(
                LogMsg() << " note has cross-staffing to a staff (" << noteStaff << ") that is not included in the MNX part.", MessageSeverity::Info);
        }
    }
    createTies(context, mnxNote, musxNote);
    deferJumpTies(context, musxNote);
}

static void createNotes(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    for (size_t x = 0; x < musxEntry->notes.size(); x++) {
        const auto mnxNote = NoteInfoPtr(musxEntryInfo, x);
        if (const auto percNoteInfo = mnxNote.calcPercussionNoteInfo()) {
            createNote<mnxdom::sequence::KitNote>(context, mnxEvent, mnxNote, musxStaff, percNoteInfo);
        } else {
            createNote<mnxdom::sequence::Note>(context, mnxEvent, mnxNote, musxStaff, nullptr);
        }
    }
}

static void createRest([[maybe_unused]] const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo,
    const MusxInstance<others::Staff>& musxStaff)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    auto mnxRest = mnxEvent.ensure_rest();
    // If a rest is hidden, it has been detected as a beam workaround, so its staff position is meaningless
    if (!musxEntry->isHidden && !musxEntry->floatRest) {
        const auto staffPosition =
            musxEntry->notes.empty() ? musxEntryInfo.calcZeroNotePosition() : NoteInfoPtr(musxEntryInfo, 0).calcNoteProperties().staffPosition;
        auto adjustedStaffPosition = staffPosition;
        adjustedStaffPosition += calcFinaleToSmuflRestPositionOffset(std::get<0>(musxEntry->calcDurationInfo()));
        mnxRest.set_staffPosition(mnxStaffPosition(musxStaff, adjustedStaffPosition));
    }
}

static void createFullMeasureRest(const MnxMusxMappingPtr& context, mnxdom::sequence::SequenceContent content, const EntryInfoPtr& musxEntryInfo,
    const musx::util::Fraction& measureDuration)
{
    auto sequence = content.getEnclosingElement<mnxdom::Sequence>();
    if (!sequence) {
        context->logMessage(LogMsg() << " full measure rest could not be assigned to a top-level sequence.", MessageSeverity::Warning);
        return;
    }

    auto fullMeasure = sequence->ensure_fullMeasure();
    const auto musxEntry = musxEntryInfo->getEntry();
    context->entryTargetByNumber.insert_or_assign(musxEntry->getEntryNumber(), EntryTarget{EntryTargetKind::FullMeasureRest, fullMeasure.pointer()});
    if (!musxEntry->isHidden && !musxEntry->floatRest) {
        if (const auto musxStaff = musxEntryInfo.createCurrentStaff()) {
            const auto staffPosition =
                musxEntry->notes.empty() ? musxEntryInfo.calcZeroNotePosition() : NoteInfoPtr(musxEntryInfo, 0).calcNoteProperties().staffPosition;
            const auto adjustedStaffPosition = staffPosition + calcFinaleToSmuflRestPositionOffset(NoteType::Whole);
            fullMeasure.set_staffPosition(mnxStaffPosition(musxStaff, adjustedStaffPosition));
        }
    }
    // Finale's full-measure rest is a whole rest, and it draws one even where a breve rest is the
    // convention, so the glyph is stated for measures that long.
    if (measureDuration >= musx::util::Fraction::fromEdu(Edu(NoteType::Breve))) {
        fullMeasure.ensure_visualDuration(mnxdom::NoteValueBase::Whole, 0);
    }
    processArticulations(context, fullMeasure, musxEntryInfo);
    content.clear();
}

static void createLyrics(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    auto createLyricsType = [&](const auto& musxLyrics) {
        using PtrType = typename std::decay_t<decltype(musxLyrics)>::value_type;
        using T = typename PtrType::element_type;
        static_assert(std::is_base_of_v<details::LyricAssign, T>, "musxLyrics must be a subtype of LyricAssign");
        for (const auto& lyr : musxLyrics) {
            if (auto lyrText = lyr->getLyricText()) {
                if (lyr->syllable > lyrText->syllables.size()) { // Finale syllable numbers are 1-based.
                    context->logMessage(LogMsg() << " Layer " << musxEntryInfo.getLayerIndex() + 1 << " Entry index "
                                                 << musxEntryInfo.getIndexInFrame() << " has an invalid syllable number (" << lyr->syllable << ").",
                        MessageSeverity::Warning);
                } else {
                    auto mnxLyrics = mnxEvent.ensure_lyrics();
                    auto mnxLyricsLines = mnxLyrics.ensure_lines();
                    const size_t sylIndex = size_t(lyr->syllable - 1); // Finale syllable numbers are 1-based.
                    auto mnxLyricLine = mnxLyricsLines.append(
                        calcLyricLineId(std::string(T::TextType::XmlNodeName), lyr->lyricNumber), lyrText->syllables[sylIndex]->syllable);
                    mnxLyricLine.set_type(mnxLineTypeFromLyric(lyrText->syllables[sylIndex]));
                    if (const auto wordExtension = classify::classifyLyricWordExtension(lyr)) {
                        // MNX has no word extension yet, so the line takes the id the gap anchors to. The id is
                        // written whether or not a report was requested; see design-decisions.md.
                        mnxLyricLine.set_id(core::calcLyricAssignId(lyr, musxEntryInfo));
                        if (gapCollectorFor(context)) {
                            context->deferredLyricExtensionGaps.push_back(
                                {wordExtension, classify::GapAnchor{mnxLyricLine.id_or(""), std::nullopt, std::nullopt}});
                        }
                    }
                }
            }
        }
    };
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignVerse>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignChorus>(SCORE_PARTID, musxEntry->getEntryNumber()));
    createLyricsType(musxEntry->getDocument()->getDetails()->getArray<details::LyricAssignSection>(SCORE_PARTID, musxEntry->getEntryNumber()));
}

void finalizeLyricExtensionGaps(const MnxMusxMappingPtr& context)
{
    auto* const gapCollector = gapCollectorFor(context);
    if (!gapCollector) {
        context->deferredLyricExtensionGaps.clear();
        return;
    }
    for (auto& deferred : context->deferredLyricExtensionGaps) {
        const auto& endEntry = deferred.classification.endEntry;
        if (!endEntry) {
            gapCollector->add(std::move(deferred.start), std::move(deferred.classification), classify::GapExtent::Partial);
            continue;
        }
        const auto endEntryNumber = endEntry->getEntry()->getEntryNumber();
        std::optional<classify::GapAnchor> end;
        const auto targetIt = context->entryTargetByNumber.find(endEntryNumber);
        if (targetIt != context->entryTargetByNumber.end() && targetIt->second.kind == EntryTargetKind::Event) {
            end = classify::GapAnchor{core::calcEventId(endEntry), std::nullopt, std::nullopt};
        } else {
            // The end entry was not exported (a cue layer, or a full-measure rest with no event id), so
            // the end names the measure it falls in.
            end = partMeasureAnchor(context, endEntry.getStaff(), endEntry.getMeasure(), endEntry->elapsedDuration);
        }
        if (end) {
            gapCollector->addSpan(std::move(deferred.start), std::move(*end), std::move(deferred.classification), classify::GapExtent::Partial);
        } else {
            gapCollector->add(std::move(deferred.start), std::move(deferred.classification), classify::GapExtent::Partial);
        }
    }
    context->deferredLyricExtensionGaps.clear();
}

static std::optional<mnxdom::sequence::Event> createEvent(const MnxMusxMappingPtr& context, mnxdom::sequence::SequenceContent content,
    EntryInfoPtr musxEntryInfo, bool effectiveHidden, bool hasVoice1Voice2, const MusxInstance<details::TupletDef>& tupletDef, bool forTremolo)
{
    const auto musxEntry = musxEntryInfo->getEntry();

    if (effectiveHidden) {
        if (musxEntry->graceNote) {
            context->logMessage(
                LogMsg() << "Skipping hidden entry " << musxEntry->getEntryNumber() << " in an MNX grace-note run.", MessageSeverity::Info);
            return std::nullopt;
        }
        /// @todo include hidden entries perhaps, if MNX starts allowing them.
        content.appendSpace(mnxFractionFromEdu(musxEntry->duration));
        return std::nullopt;
    }

    if (musxEntry->isNote && musxEntry->notes.empty()) {
        if (musxEntry->graceNote) {
            context->logMessage(
                LogMsg() << "Skipping zero-note entry " << musxEntry->getEntryNumber() << " in an MNX grace-note run.", MessageSeverity::Info);
            return std::nullopt;
        }
        context->logMessage(LogMsg() << "Emitting zero-note entry " << musxEntry->getEntryNumber() << " as an MNX spacer.", MessageSeverity::Info);
        content.appendSpace(mnxFractionFromEdu(musxEntry->duration));
        return std::nullopt;
    }

    auto musxStaff = musxEntryInfo.createCurrentStaff();
    if (!musxStaff) {
        throw std::invalid_argument("Entry " + std::to_string(musxEntry->getEntryNumber()) + " has no staff information for staff "
                                    + std::to_string(musxEntryInfo.getStaff()));
    }

    Edu effectiveDura = musxEntry->duration;
    if (forTremolo && tupletDef) {
        effectiveDura = tupletDef->calcReferenceDuration().calcEduDuration();
    }
    const auto noteValue = mnxNoteValueFromEdu(effectiveDura);
    auto mnxEvent = content.appendEvent(noteValue.base, noteValue.dots);
    mnxEvent.set_id(core::calcEventId(musxEntryInfo));
    context->entryTargetByNumber.insert_or_assign(musxEntry->getEntryNumber(), EntryTarget{EntryTargetKind::Event, mnxEvent.pointer()});
    createLyrics(context, mnxEvent, musxEntryInfo);
    processArticulations(context, mnxEvent, musxEntryInfo);
    processEntrySmartShapes(context, mnxEvent, musxEntryInfo);
    if (const auto& crossedStaffId = musxEntryInfo.calcCrossedStaffForAll()) {
        if (const auto& mnxPartStaff = context->mnxPartStaffFromStaff(crossedStaffId.value())) {
            mnxEvent.set_staff(mnxPartStaff.value());
        } else {
            context->logMessage(
                LogMsg() << " entry has cross-staffing to a staff (" << crossedStaffId.value() << ") that is not included in the MNX part.",
                MessageSeverity::Info);
        }
    }
    const auto [freezeStem, upStem] = musxEntryInfo.calcEntryStemSettings();
    if (musxEntry->isNote && musxEntry->hasStem()) {
        if (freezeStem) {
            mnxEvent.set_stemDirection(upStem ? mnxdom::StemDirection::Up : mnxdom::StemDirection::Down);
        } else if (hasVoice1Voice2) {
            // force all stems in v1v2 contexts due to voices controlling stem direction otherwise.
            mnxEvent.set_stemDirection(musxEntryInfo.calcUpStem() ? mnxdom::StemDirection::Up : mnxdom::StemDirection::Down);
        }
    }

    if (musxEntry->isNote) {
        createNotes(context, mnxEvent, musxEntryInfo, musxStaff);
    } else {
        createRest(context, mnxEvent, musxEntryInfo, musxStaff);
    }
    return mnxEvent;
}

/// @brief processes as many entries as it can and returns the next entry to process up to the caller
static EntryInfoPtr::InterpretedIterator addEntryToContent(const MnxMusxMappingPtr& context, mnxdom::sequence::SequenceContent content,
    const EntryInfoPtr::InterpretedIterator& firstEntryInfo, musx::util::Fraction& elapsedInSequence, bool hasVoice1Voice2, bool inGrace,
    const std::optional<size_t>& tupletIndex = std::nullopt, bool inTremolo = false)
{
    auto next = firstEntryInfo;
    while (next) {
        if (tupletIndex) {
            const auto& currentTuplet = next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()];
            if (next.getEntryInfo().getIndexInFrame() > currentTuplet.endIndex) {
                // We've already processed the last event of this tuplet
                // (possibly inside a nested tuplet call) – hand back control.
                return next;
            }
        }

        const auto entryInfo = next.getEntryInfo();
        const auto entry = entryInfo->getEntry();
        if (inGrace && !entry->graceNote) {
            return next;
        } else if (!inGrace && entry->graceNote) {
            auto grace = content.appendGrace();
            next = addEntryToContent(context, grace.content(), next, elapsedInSequence, hasVoice1Voice2, true);
            grace.set_or_clear_slash(entryInfo.calcGraceNoteSlash(context->finaleOptions.graceOptions));
            continue;
        }

        if (next.calcIsPastLogicalEndOfFrame()) {
            return {};
        }
        const auto currElapsedDuration = next.getEffectiveElapsedDuration();
        const auto measureDuration = next.getEffectiveMeasureStaffDuration();
        if (currElapsedDuration >= measureDuration) {
            if (currElapsedDuration > measureDuration) {
                if (auto prev = next.getPrevious(); prev && prev.getEffectiveElapsedDuration() < next.getEffectiveMeasureStaffDuration()) {
                    context->logMessage(LogMsg() << "Entry " << prev.getEntryInfo()->getEntry()->getEntryNumber() << " at index "
                                                 << prev.getEntryInfo().getIndexInFrame() << " exceeds the measure length.",
                        MessageSeverity::Warning);
                }
            }
            if (tupletIndex) { // keep tuplets together, even if they exceed the measure
                context->logMessage(LogMsg() << "Tuplet exceeds the measure length. This is not supported in MNX. Results may be unpredictable.",
                    MessageSeverity::Warning);
            }
        }

        ASSERT_IF (currElapsedDuration < elapsedInSequence) {
            throw std::logic_error("Next entry's elapsed duration value is smaller than tracked duration for sequence.");
        }
        if (currElapsedDuration > elapsedInSequence) {
            content.appendSpace(mnxFractionFromFraction(currElapsedDuration - elapsedInSequence));
            elapsedInSequence = currElapsedDuration;
        }
        if (context->currSplitInstrumentUuid) {
            const auto& instInfo = context->document->getInstrumentForStaff(context->current.staff);
            const auto identity = instInfo.getInstrumentIdentityAt(MusicPoint(entryInfo.getMeasure(), currElapsedDuration));
            if (identity.instUuid != context->currSplitInstrumentUuid.value()) {
                context->logMessage(LogMsg() << "Entry " << entry->getEntryNumber()
                                             << " has an instrument identity that differs from the active split part inside measure "
                                             << entryInfo.getMeasure() << ". Emitting it in the current frame's part.",
                    MessageSeverity::Warning);
            }
        }

        if (tupletIndex) {
            auto tuplInfo = next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()];
            if (tuplInfo.endIndex == next.getEntryInfo().getIndexInFrame()) {
                auto thisTupletIndex = next.getEntryInfo().calcNextTupletIndex(tupletIndex);
                if (!thisTupletIndex
                    || next.getEntryInfo().getFrame()->tupletInfo[thisTupletIndex.value()].startIndex != next.getEntryInfo().getIndexInFrame()) {
                    createEvent(context, content, next.getEntryInfo(), next.getEffectiveHidden(), hasVoice1Voice2, tuplInfo.tuplet, inTremolo);
                    elapsedInSequence = currElapsedDuration + next.getEntryInfo()->actualDuration;
                    return next.getNext();
                }
            }
        }

        if (!inGrace) {
            auto thisTupletIndex = next.getEntryInfo().calcNextTupletIndex(tupletIndex);
            if (thisTupletIndex != tupletIndex && thisTupletIndex) {
                auto tuplInfo = next.getEntryInfo().getFrame()->tupletInfo[thisTupletIndex.value()];
                if (isOmittedZeroLengthTuplet(tuplInfo)) {
                    context->discardedZeroLengthTuplets++;
                    context->logMessage(LogMsg() << "Entry " << entry->getEntryNumber()
                                                 << " starts a zero-length tuplet, which MNX cannot represent; its entries are omitted.",
                        MessageSeverity::Verbose);
                    while (next && next.getEntryInfo().getIndexInFrame() <= tuplInfo.endIndex) {
                        next = next.getNext();
                    }
                    continue;
                }
                if (tuplInfo.calcIsTremolo()) {
                    const auto numBeams = next.getEntryInfo().calcNumberOfBeams();
                    const auto numFlagsInRef = calcNumberOfBeamsInEdu(tuplInfo.tuplet->calcReferenceDuration().calcEduDuration());
                    if (numFlagsInRef >= numBeams) {
                        context->logMessage(
                            LogMsg() << "not enough flags or beams to create a tremolo. Setting tremolo marks to 1.", MessageSeverity::Warning);
                    }
                    const int marks = static_cast<int>(numFlagsInRef < numBeams ? numBeams - numFlagsInRef : 0);
                    auto tremolo = createMultiNoteTremolo(content, tuplInfo, marks);
                    next = addEntryToContent(
                        context, tremolo.content(), next, elapsedInSequence, hasVoice1Voice2, inGrace, thisTupletIndex, /*inTremolo*/ true);
                    continue;
                } else {
                    auto tuplet = createTuplet(context, content, tuplInfo, next.getEntryInfo());
                    next = addEntryToContent(context, tuplet.content(), next, elapsedInSequence, hasVoice1Voice2, inGrace, thisTupletIndex);
                    continue;
                }
            }
        }

        auto tupletDef = [&]() -> MusxInstance<details::TupletDef> {
            if (tupletIndex) {
                return next.getEntryInfo().getFrame()->tupletInfo[tupletIndex.value()].tuplet;
            }
            return nullptr;
        }();

        const bool fullMeasureRest = next.getEntryInfo().calcIsFullMeasureRest();
        if (fullMeasureRest) {
            createFullMeasureRest(context, content, next.getEntryInfo(), next.getEffectiveMeasureStaffDuration());
            elapsedInSequence = currElapsedDuration + next.getEffectiveMeasureStaffDuration();
        } else {
            createEvent(context, content, next.getEntryInfo(), next.getEffectiveHidden(), hasVoice1Voice2, tupletDef, inTremolo);
            elapsedInSequence = currElapsedDuration + next.getEffectiveActualDuration();
        }

        next = next.getNext();
        if (!fullMeasureRest && inGrace && next) {
            const auto nextInfo = next.getEntryInfo();
            if (nextInfo.calcUnbeamed() || nextInfo.calcIsBeamStart()) {
                // each beam group should be coded as a separate grace note sequence, so that we can control
                // the slashes.
                break;
            }
        }
    }
    return next;
}

/// @brief Appends the full-measure rest an empty staff measure displays, if the staff displays one.
/// See "An empty staff measure is written as a full-measure rest" in design-decisions.md.
static void appendEmptyMeasureRest(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure)
{
    const auto musxStaff = others::StaffComposite::createCurrent(
        musxMeasure->getDocument(), musxMeasure->getRequestedPartId(), context->current.staff, musxMeasure->getCmper(), 0);
    // Asking about the alternate notation's own layer reduces the question to whether that notation
    // replaces entries at all.
    if (!musxStaff || musxStaff->blankMeasure || musxStaff->calcAlternateNotationHidesEntries(musxStaff->altLayer)) {
        return;
    }
    auto sequence = mnxMeasure.sequences().append();
    sequence.set_or_clear_staff(mnxStaffNumber.value_or(1));
    sequence.ensure_fullMeasure();
}

static void createEntrySequences(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure)
{
    if (!context->current.staffMeasureContext || !*context->current.staffMeasureContext) {
        return; // nothing to do
    }
    if (context->current.cuePlan.isDetectedCueOnly) {
        return;
    }
    const auto measureDuration = musxMeasure->calcDuration(context->current.staff);
    for (const auto& [layer, numV2] : context->current.layerVoices) {
        if (context->current.cuePlan.isCueLayer(layer)) {
            continue;
        }
        // Finale's alternate notation draws something else in place of these entries, so exporting them
        // would duplicate music that the source does not show.
        /// @todo Measure repeats are exported in place of the omitted entries, and blank notation shows
        /// nothing, but slash notation has no MNX representation, so those measures lose their music
        /// entirely. Export the slashes here if MNX gains a way to encode them.
        if (context->current.layersHiddenByAltNotation.contains(layer)) {
            continue;
        }
        const int maxVoices = numV2 ? 2 : 1;
        if (auto entryFrame = context->current.staffMeasureContext->createEntryFrame(layer)) {
            const bool usesV1V2 = numV2 && entryFrame->getFirstInterpretedIterator(2); // ignore entries the iterator will skip
            auto entries = entryFrame->getEntries();
            if (!entries.empty()) {
                for (int voice = 1; voice <= maxVoices; voice++) {
                    if (auto firstEntry = entryFrame->getFirstInterpretedIterator(voice)) {
                        auto sequence = mnxMeasure.sequences().append();
                        context->current.voice = calcVoice(mnxStaffNumber.value_or(1), layer, voice);
                        sequence.set_staff(mnxStaffNumber.value_or(1));
                        sequence.set_voice(context->current.voice);
                        // Finale's layer stem setting is the hint. A v2 sequence is stemmed per v2 launch, so it has none.
                        if (voice == 1) {
                            const auto layerAtts = entryFrame->getLayerAttributes();
                            if (layerAtts && layerAtts->freezeLayer && firstEntry.getEntryInfo().calcIfLayerSettingsApply()) {
                                sequence.set_or_clear_directionHint(
                                    layerAtts->freezeStemsUp ? mnxdom::DirectionHint::Upper : mnxdom::DirectionHint::Lower);
                            }
                        }
                        auto elapsedInVoice = musx::util::Fraction(0);
                        addEntryToContent(context, sequence.content(), firstEntry, elapsedInVoice, usesV1V2, false);
                        appendMeasureRemainderSpaces(sequence.content(), elapsedInVoice, measureDuration);
                        context->current.voice.clear();
                    }
                }
            }
        }
    }
}

void createSequences(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure)
{
    const size_t sequenceCountBefore = mnxMeasure.sequences().size();
    createEntrySequences(context, mnxMeasure, mnxStaffNumber, musxMeasure);
    if (mnxMeasure.sequences().size() == sequenceCountBefore) {
        appendEmptyMeasureRest(context, mnxMeasure, mnxStaffNumber, musxMeasure);
    }
}

void finalizeEntryTargets(const MnxMusxMappingPtr& context)
{
    const auto isExported = [&](EntryNumber entryNumber) {
        const auto it = context->entryTargetByNumber.find(entryNumber);
        return it != context->entryTargetByNumber.end() && it->second.kind == EntryTargetKind::Event;
    };
    size_t releasedTies = 0;
    for (const auto& deferred : context->deferredTieTargets) {
        if (isExported(deferred.targetEntry)) {
            continue;
        }
        // The tied-to note is not in the document, so the tie hangs from its start the way Finale draws it.
        mnxdom::sequence::Tie tie(context->mnxDocument->root(), deferred.pointer);
        tie.clear_target();
        tie.clear_targetType();
        tie.set_lv(true);
        releasedTies++;
    }
    context->deferredTieTargets.clear();

    // Removing a slur shifts the later slurs of the same event, so each event's slurs are removed from the back.
    std::map<mnxdom::json_pointer, std::vector<size_t>> danglingSlurs;
    for (auto& deferred : context->deferredSlurTargets) {
        if (isExported(deferred.target.targetEntry)) {
            continue;
        }
        mnxdom::sequence::Slur slur(context->mnxDocument->root(), deferred.target.pointer);
        danglingSlurs[deferred.target.pointer.parent_pointer()].push_back(slur.calcArrayIndex());
        // Reported like any other slur MNX cannot carry; finalizeSmartShapeGaps anchors its end.
        if (gapCollectorFor(context) && !deferred.shape->hidden) {
            const auto startNote = deferred.shape->calcStartNote();
            classify::GapAnchor start{
                startNote ? core::calcNoteId(startNote) : core::calcEventId(deferred.shape->startTermSeg->endPoint->calcAssociatedEntry()),
                std::nullopt, std::nullopt};
            context->deferredSmartShapeGaps.push_back({std::move(deferred.shape), std::move(deferred.classification), std::move(start)});
        }
    }
    context->deferredSlurTargets.clear();
    size_t removedSlurs = 0;
    for (auto& [slursPointer, indices] : danglingSlurs) {
        mnxdom::Array<mnxdom::sequence::Slur> slurs(context->mnxDocument->root(), slursPointer);
        std::sort(indices.begin(), indices.end(), std::greater<size_t>());
        for (const size_t index : indices) {
            slurs.erase(index);
            removedSlurs++;
        }
        if (slurs.size() == 0) {
            slurs.parent<mnxdom::sequence::Event>().clear_slurs();
        }
    }

    if (releasedTies > 0 || removedSlurs > 0) {
        context->logMessage(
            LogMsg() << "Released " << releasedTies << " ties and removed " << removedSlurs << " slurs whose target entry was not exported.",
            MessageSeverity::Verbose);
    }
}

void finalizeJumpTies(const MnxMusxMappingPtr& context)
{
    if (context->deferredJumpTies.empty()) {
        return;
    }

    std::unordered_set<std::string> clearedLvTies;
    std::unordered_map<std::string, mnxdom::SlurTieSide> consensusSides;
    for (const auto& deferred : context->deferredJumpTies) {
        const auto noteIt = context->noteJsonById.find(deferred.startNoteId);
        if (noteIt == context->noteJsonById.end()) {
            continue;
        }

        mnxdom::sequence::NoteBase startNote(context->mnxDocument->root(), noteIt->second);
        // The side the note's other ties agree on, or Auto if any of them leaves it open or they disagree.
        const auto consensusSide = [&]() -> mnxdom::SlurTieSide {
            if (const auto cached = consensusSides.find(deferred.startNoteId); cached != consensusSides.end()) {
                return cached->second;
            }
            auto side = mnxdom::SlurTieSide::Auto;
            if (auto tiesOpt = startNote.ties()) {
                auto ties = tiesOpt.value();
                for (size_t i = 0; i < ties.size(); i++) {
                    auto tie = ties.at(i);
                    if (tie.lv()) {
                        continue;
                    }
                    if (tie.side() == mnxdom::SlurTieSide::Auto || (side != mnxdom::SlurTieSide::Auto && side != tie.side())) {
                        side = mnxdom::SlurTieSide::Auto;
                        break;
                    }
                    side = tie.side();
                }
            }
            consensusSides.emplace(deferred.startNoteId, side);
            return side;
        }();
        if (clearedLvTies.insert(deferred.startNoteId).second) {
            if (auto tiesOpt = startNote.ties()) {
                auto ties = tiesOpt.value();
                for (size_t i = ties.size(); i-- > 0;) {
                    if (ties.at(i).lv()) {
                        ties.erase(i);
                    }
                }
                if (ties.size() == 0) {
                    startNote.clear_ties();
                }
            }
        }

        auto mnxTies = startNote.ensure_ties();
        bool alreadyLinked = false;
        for (size_t i = 0; i < mnxTies.size(); i++) {
            auto tie = mnxTies.at(i);
            if (tie.target() && tie.target().value() == deferred.endNoteId) {
                alreadyLinked = true;
                break;
            }
        }
        if (alreadyLinked) {
            continue;
        }

        auto mnxTie = mnxTies.append();
        mnxTie.set_target(deferred.endNoteId);
        mnxTie.set_targetType(mnxdom::TieTargetType::CrossJump);
        mnxTie.set_or_clear_side(deferred.side != mnxdom::SlurTieSide::Auto ? deferred.side : consensusSide);
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
