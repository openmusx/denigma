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
#include "gtest/gtest.h"

#include <memory>
#include <string>

#include "core/musx_reader.h"
#include "denigma/classify/percussion.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "input/input.h"
#include "musx/musx.h"
#include "test_playback_resources.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

DocumentPtr loadFixture(const std::string& fileName, const std::shared_ptr<const denigma::MusResources>& musResources = nullptr)
{
    setupTestDataPaths();
    const auto inputPath = getInputPath() / fileName;
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    denigmaContext.musResources = musResources;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

// The percussion note of map @p percussionMapId whose note type is @p noteTypeId.
MusxInstance<others::PercussionNoteInfo> findPercussionNote(const DocumentPtr& document, Cmper percussionMapId, PercussionNoteTypeId noteTypeId)
{
    for (const auto& percussionNote : document->getOthers()->getArray<others::PercussionNoteInfo>(SCORE_PARTID, percussionMapId)) {
        if (percussionNote->percNoteType == noteTypeId) {
            return percussionNote;
        }
    }
    return nullptr;
}

std::shared_ptr<others::PercussionNoteInfo> makePercussionNote(const DocumentPtr& document, PercussionNoteTypeId noteTypeId)
{
    auto result = std::make_shared<others::PercussionNoteInfo>(document, SCORE_PARTID, EnigmaBase::ShareMode::All, Cmper{1}, Inci{0});
    result->percNoteType = noteTypeId;
    return result;
}

constexpr PercussionNoteTypeId secondCopy(PercussionNoteTypeId baseNoteTypeId)
{
    constexpr PercussionNoteTypeId orderIdShift = 12;
    return PercussionNoteTypeId(baseNoteTypeId | (1 << orderIdShift));
}

} // namespace

TEST(PercussionNoteClassification, NullPercussionNoteIsEmpty)
{
    EXPECT_FALSE(classifyPercussionNote(MusxInstance<others::PercussionNoteInfo>{}));
}

TEST(PercussionNoteClassification, CopiesOfANoteTypeAreDistinctSounds)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    // Map 55 has both copies of note type 8, Triangle Mute.
    constexpr Cmper percussionMapId = 55;
    constexpr PercussionNoteTypeId triangleMute = 8;
    const auto first = classifyPercussionNote(findPercussionNote(document, percussionMapId, triangleMute));
    const auto second = classifyPercussionNote(findPercussionNote(document, percussionMapId, secondCopy(triangleMute)));
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);

    EXPECT_EQ(first.mapNote.percussionMapId, percussionMapId);
    EXPECT_EQ(first.mapNote.noteTypeId, triangleMute);
    EXPECT_EQ(second.mapNote.noteTypeId, secondCopy(triangleMute));
    EXPECT_NE(first.mapNote, second.mapNote);

    EXPECT_EQ(first.sound.baseNoteTypeId, triangleMute);
    EXPECT_EQ(first.sound.orderId, 0u);
    EXPECT_EQ(second.sound.baseNoteTypeId, triangleMute);
    EXPECT_EQ(second.sound.orderId, 1u);
    EXPECT_LT(first.sound, second.sound);

    EXPECT_EQ(first.name, "Triangle Mute");
    EXPECT_EQ(first.generalMidiKey, 80);
    EXPECT_EQ(first.staffReferencePosition, first.percussionNote->calcStaffReferencePosition());
}

TEST(PercussionNoteClassification, SameNoteTypeInDifferentMapsIsOneSound)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    constexpr PercussionNoteTypeId kickDrum = 2;
    MusxInstance<others::PercussionNoteInfo> firstKick;
    MusxInstance<others::PercussionNoteInfo> otherKick;
    for (const auto& percussionNote : document->getOthers()->getArray<others::PercussionNoteInfo>(SCORE_PARTID)) {
        if (percussionNote->percNoteType != kickDrum) {
            continue;
        }
        if (!firstKick) {
            firstKick = percussionNote;
        } else if (percussionNote->getCmper() != firstKick->getCmper()) {
            otherKick = percussionNote;
            break;
        }
    }
    ASSERT_TRUE(firstKick);
    ASSERT_TRUE(otherKick);

    const auto first = classifyPercussionNote(firstKick);
    const auto other = classifyPercussionNote(otherKick);
    EXPECT_NE(first.mapNote, other.mapNote);
    EXPECT_EQ(first.sound, other.sound);
    EXPECT_EQ(first.name, "Kick Drum");
    EXPECT_EQ(first.generalMidiKey, 36);
}

TEST(PercussionNoteClassification, NamesNumberLaterCopies)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    // Snare Drum LH names its copy and has no General MIDI key.
    constexpr PercussionNoteTypeId snareDrumLeftHand = 236;
    const auto classification = classifyPercussionNote(makePercussionNote(document, secondCopy(snareDrumLeftHand)));
    ASSERT_TRUE(classification);
    EXPECT_EQ(classification.name, "Snare Drum (2) LH");
    EXPECT_FALSE(classification.generalMidiKey);
}

TEST(PercussionNoteClassification, UnknownNoteTypeHasNoName)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    const auto classification = classifyPercussionNote(makePercussionNote(document, PercussionNoteTypeId{0}));
    ASSERT_TRUE(classification);
    EXPECT_FALSE(classification.name);
    EXPECT_FALSE(classification.generalMidiKey);
}

TEST(PercussionNoteClassification, UnmappedNoteOnAPercussionStaffPlaysItsPitch)
{
    // Entry 131 of drumset.musx is on the percussion staff but has no percussion note code. Finale plays it
    // as its pitch, D-sharp 5, which is MIDI key 75.
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);
    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 131);
    ASSERT_TRUE(entryInfo);
    const NoteInfoPtr note(entryInfo, 0);
    const auto classification = classifyPercussionNote(note);
    EXPECT_TRUE(classification.isOnPercussionStaff);
    EXPECT_FALSE(classification);
    EXPECT_FALSE(classification.name);
    EXPECT_FALSE(classification.generalMidiKey);
    EXPECT_EQ(classification.unmappedMidiKey, 75);
    EXPECT_EQ(classification.staffReferencePosition, note.calcNoteProperties().staffPosition);
}

TEST(PercussionNoteClassification, MappedNoteHasNoUnmappedKey)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);
    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 125);
    ASSERT_TRUE(entryInfo);
    const auto classification = classifyPercussionNote(NoteInfoPtr(entryInfo, 0));
    EXPECT_TRUE(classification.isOnPercussionStaff);
    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.name, "Kick Drum");
    EXPECT_FALSE(classification.unmappedMidiKey);
}

TEST(PercussionNoteClassification, NoteOffAPercussionStaffIsEmpty)
{
    // Measure 2 of drumset.musx has a staff style that makes the staff an ordinary pitched staff.
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);
    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 128);
    ASSERT_TRUE(entryInfo);
    const auto classification = classifyPercussionNote(NoteInfoPtr(entryInfo, 0));
    EXPECT_FALSE(classification.isOnPercussionStaff);
    EXPECT_FALSE(classification);
    EXPECT_FALSE(classification.unmappedMidiKey);
}

namespace {

// drumset.musx's mapped notes: entry 125 is a kick drum (note type 2), entry 126 a low woodblock (11).
constexpr PercussionNoteTypeId kickDrum = 2;
constexpr PercussionNoteTypeId highWoodblock = 10;
constexpr EntryNumber kickDrumEntry = 125;
constexpr EntryNumber lowWoodblockEntry = 126;

PercussionNoteClassification classifyDrumsetNote(const DocumentPtr& document, EntryNumber entryNumber)
{
    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, entryNumber);
    EXPECT_TRUE(entryInfo) << entryNumber;
    return entryInfo ? classifyPercussionNote(NoteInfoPtr(entryInfo, 0)) : PercussionNoteClassification{};
}

} // namespace

TEST(PercussionNoteClassification, DevicePlaysItsOwnKeys)
{
    const auto document =
        loadFixture("drumset.musx", denigma::test::createSyntheticPlaybackResources(denigma::test::DRUMSET_SOUND_UUID, {{{kickDrum, 40}}}));
    ASSERT_TRUE(document);
    const auto kick = classifyDrumsetNote(document, kickDrumEntry);
    EXPECT_EQ(kick.playbackMidiKey, 40);
    EXPECT_FALSE(kick.playbackLookupFailure);
    EXPECT_TRUE(kick.isPlayedByDevice());
    // The device's key wins over the note type's General MIDI key, 36.
    EXPECT_EQ(kick.generalMidiKey, 36);
    EXPECT_EQ(kick.calcMidiKey(), 40);
}

TEST(PercussionNoteClassification, UnlistedNoteTypePlaysItsParentInstrument)
{
    // The low woodblock's parent instrument is the high woodblock, which is all the device lists.
    const auto document = loadFixture(
        "drumset.musx", denigma::test::createSyntheticPlaybackResources(denigma::test::DRUMSET_SOUND_UUID, {{{kickDrum, 40}, {highWoodblock, 50}}}));
    ASSERT_TRUE(document);
    const auto woodblock = classifyDrumsetNote(document, lowWoodblockEntry);
    EXPECT_EQ(woodblock.sound.baseNoteTypeId, 11);
    EXPECT_EQ(woodblock.playbackMidiKey, 50);
    EXPECT_FALSE(woodblock.playbackLookupFailure);
    EXPECT_TRUE(woodblock.isPlayedByDevice());
    EXPECT_EQ(woodblock.calcMidiKey(), 50);
}

TEST(PercussionNoteClassification, DeviceThatListsNoAncestorDoesNotPlayTheNote)
{
    const auto document =
        loadFixture("drumset.musx", denigma::test::createSyntheticPlaybackResources(denigma::test::DRUMSET_SOUND_UUID, {{{kickDrum, 40}}}));
    ASSERT_TRUE(document);
    const auto woodblock = classifyDrumsetNote(document, lowWoodblockEntry);
    EXPECT_TRUE(woodblock);
    EXPECT_FALSE(woodblock.playbackMidiKey);
    EXPECT_EQ(woodblock.playbackLookupFailure, PlaybackLookupFailure::NoteTypeNotListed);
    EXPECT_FALSE(woodblock.isPlayedByDevice());
}

TEST(PercussionNoteClassification, GeneralMidiStandsInWithoutADeviceList)
{
    // Without resources, and with a sound that names no percussion map, the note type's General MIDI key is used.
    const auto withoutResources = loadFixture("drumset.musx");
    const auto withoutList =
        loadFixture("drumset.musx", denigma::test::createSyntheticPlaybackResources(denigma::test::DRUMSET_SOUND_UUID, std::nullopt));
    ASSERT_TRUE(withoutResources);
    ASSERT_TRUE(withoutList);
    const std::pair<DocumentPtr, PlaybackLookupFailure> cases[] = {
        {withoutResources, PlaybackLookupFailure::ResourcesNotSupplied},
        {withoutList, PlaybackLookupFailure::NoPercussionMidiMap},
    };
    for (const auto& [document, failure] : cases) {
        const auto kick = classifyDrumsetNote(document, kickDrumEntry);
        EXPECT_FALSE(kick.playbackMidiKey);
        EXPECT_EQ(kick.playbackLookupFailure, failure);
        EXPECT_TRUE(kick.isPlayedByDevice());
        EXPECT_EQ(kick.calcMidiKey(), 36);
    }
}
