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

#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/lyrics.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

/// Loads lyric_placeholders.enigmaxml, derived from lyric_legacy_wext.musx. Its added Verse 2 is a real
/// syllable ("as", syllable 2) among lone non-breaking spaces (syllables 1 and 3-6), assigned as:
/// ev17 syllables 1 then 2; ev18 syllable 3; ev23 syllable 4 with a word extension; ev25 syllable 5 with
/// a word extension, then syllable 6. Word extensions are legacy, so their ends are inferred.
DocumentPtr loadPlaceholderFixture()
{
    setupTestDataPaths();
    std::vector<char> xml;
    readFile(getInputPath() / "lyric_placeholders.enigmaxml", xml);
    return musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
}

EntryLyricAssignments<details::LyricAssignVerse> classifyVerses(const DocumentPtr& document, EntryNumber entryNumber)
{
    return classifyLyricAssignments(document->getDetails()->getArray<details::LyricAssignVerse>(SCORE_PARTID, entryNumber));
}

const LyricLineAssignment<details::LyricAssignVerse>* findLine(const EntryLyricAssignments<details::LyricAssignVerse>& classified, Cmper lyricNumber)
{
    for (const auto& line : classified.lines) {
        if (line.assignment->lyricNumber == lyricNumber) {
            return &line;
        }
    }
    return nullptr;
}

} // namespace

TEST(LyricClassification, OrdinarySyllablesPassThrough)
{
    const auto document = loadPlaceholderFixture();
    ASSERT_TRUE(document);
    const auto classified = classifyVerses(document, 18);
    EXPECT_TRUE(classified.invalid.empty());
    EXPECT_TRUE(classified.discarded.empty());
    const auto* verse1 = findLine(classified, 1);
    ASSERT_NE(verse1, nullptr);
    EXPECT_FALSE(verse1->blank);
    EXPECT_EQ(verse1->syllableIndex, 1u);
}

TEST(LyricClassification, DuplicateLinePrefersVisibleText)
{
    const auto document = loadPlaceholderFixture();
    ASSERT_TRUE(document);
    const auto classified = classifyVerses(document, 17);
    ASSERT_EQ(classified.lines.size(), 2u);
    EXPECT_EQ(classified.lines[0].assignment->lyricNumber, 1);
    const auto* verse2 = findLine(classified, 2);
    ASSERT_NE(verse2, nullptr);
    EXPECT_FALSE(verse2->blank);
    EXPECT_EQ(verse2->assignment->syllable, 2u);
    ASSERT_EQ(classified.discarded.size(), 1u);
    EXPECT_EQ(classified.discarded[0]->lyricNumber, 2);
    EXPECT_EQ(classified.discarded[0]->syllable, 1u);
}

TEST(LyricClassification, BlankSyllableWithoutExtensionIsOmitted)
{
    const auto document = loadPlaceholderFixture();
    ASSERT_TRUE(document);
    const auto classified = classifyVerses(document, 18);
    EXPECT_EQ(findLine(classified, 2), nullptr);
    EXPECT_TRUE(classified.discarded.empty());
}

TEST(LyricClassification, BlankSyllableWithExtensionIsKept)
{
    const auto document = loadPlaceholderFixture();
    ASSERT_TRUE(document);
    const auto classified = classifyVerses(document, 23);
    const auto* verse2 = findLine(classified, 2);
    ASSERT_NE(verse2, nullptr);
    EXPECT_TRUE(verse2->blank);
    EXPECT_EQ(verse2->wordExtension.kind, lyric::WordExtensionKind::Legacy);
    ASSERT_TRUE(verse2->wordExtension.endEntry);
    EXPECT_EQ(verse2->wordExtension.endEntry->getEntry()->getEntryNumber(), 24);
}

TEST(LyricClassification, AllBlankDuplicatesKeepTheFirst)
{
    const auto document = loadPlaceholderFixture();
    ASSERT_TRUE(document);
    const auto classified = classifyVerses(document, 25);
    const auto* verse2 = findLine(classified, 2);
    ASSERT_NE(verse2, nullptr);
    EXPECT_TRUE(verse2->blank);
    EXPECT_EQ(verse2->assignment->syllable, 5u);
    EXPECT_TRUE(verse2->wordExtension);
    ASSERT_EQ(classified.discarded.size(), 1u);
    EXPECT_EQ(classified.discarded[0]->syllable, 6u);
}

TEST(LyricClassification, StrippedUnderscoresAreAnExtension)
{
    // lyric_underscores.enigmaxml, derived from verse_chorus_section.musx, uses smart word extensions, so
    // underscores are stripped from syllable text. Verse 1 on ev13 is "It_" and Verse 2 a non-breaking
    // space followed by an underscore; neither assignment stores an extension of its own.
    setupTestDataPaths();
    std::vector<char> xml;
    readFile(getInputPath() / "lyric_underscores.enigmaxml", xml);
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);

    const auto classified = classifyVerses(document, 13);
    const auto* verse1 = findLine(classified, 1);
    ASSERT_NE(verse1, nullptr);
    EXPECT_FALSE(verse1->blank);
    EXPECT_EQ(verse1->wordExtension.kind, lyric::WordExtensionKind::Underscores);
    // The next syllable of each line is on the following entry, so each inferred extension ends on its own.
    ASSERT_TRUE(verse1->wordExtension.endEntry);
    EXPECT_EQ(verse1->wordExtension.endEntry->getEntry()->getEntryNumber(), 13);

    const auto* verse2 = findLine(classified, 2);
    ASSERT_NE(verse2, nullptr);
    EXPECT_TRUE(verse2->blank);
    EXPECT_EQ(verse2->wordExtension.kind, lyric::WordExtensionKind::Underscores);
    ASSERT_TRUE(verse2->wordExtension.endEntry);
    EXPECT_EQ(verse2->wordExtension.endEntry->getEntry()->getEntryNumber(), 13);
}
