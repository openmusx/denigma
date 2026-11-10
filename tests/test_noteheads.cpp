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

#include <string>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/noteheads.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "input/input.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct FontContext
{
    DocumentPtr document;
    MusxInstance<FontInfo> fontInfo;
};

static FontContext makeFontContext(const std::string& fontName = "Maestro", int charsetVal = 4095)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>)xml";
    xml += std::to_string(charsetVal);
    xml += R"xml(</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>)xml";
    xml += fontName;
    xml += R"xml(</name>
    </fontName>
  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    auto fontInfo = std::make_shared<FontInfo>(document);
    fontInfo->fontId = 1;
    fontInfo->fontSize = 24;
    return {document, fontInfo};
}

DocumentPtr loadFixture(const std::string& fileName)
{
    setupTestDataPaths();
    const auto inputPath = getInputPath() / fileName;
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

// The first percussion note in @p document whose notehead for quarter notes and shorter is @p closedNotehead.
MusxInstance<others::PercussionNoteInfo> findPercussionNote(const DocumentPtr& document, char32_t closedNotehead)
{
    for (const auto& percussionNote : document->getOthers()->getArray<others::PercussionNoteInfo>(SCORE_PARTID)) {
        if (percussionNote->closedNotehead == closedNotehead) {
            return percussionNote;
        }
    }
    return nullptr;
}

} // namespace

TEST(NoteheadClassification, ClassifiesRegularNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A4); // noteheadBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Regular);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);
    ASSERT_TRUE(filled.glyphName);
    EXPECT_EQ(filled.glyphName.value(), "noteheadBlack");

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A3); // noteheadHalf
    EXPECT_EQ(unfilled.shape, notehead::Shape::Regular);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesNullNoteheads)
{
    const auto smuflContext = makeFontContext("Finale Maestro");
    auto smuflNull = classifyNoteheadSymbol(smuflContext.fontInfo, 0xE0A5); // noteheadNull
    EXPECT_EQ(smuflNull.shape, notehead::Shape::Null);
    EXPECT_EQ(smuflNull.fill, notehead::Fill::Unspecified);
    ASSERT_TRUE(smuflNull.glyphName);
    EXPECT_EQ(smuflNull.glyphName.value(), "noteheadNull");
    EXPECT_EQ(smuflNull.noteheadInfo.font, smuflContext.fontInfo);
    EXPECT_EQ(smuflNull.noteheadInfo.character, char32_t(0xE0A5));

    const auto textContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);
    auto space = classifyNoteheadSymbol(textContext.fontInfo, U' ');
    EXPECT_EQ(space.shape, notehead::Shape::Null);
    EXPECT_EQ(space.fill, notehead::Fill::Unspecified);
    EXPECT_FALSE(space.glyphName);
    EXPECT_EQ(space.noteheadInfo.font, textContext.fontInfo);
    EXPECT_EQ(space.noteheadInfo.character, U' ');
}

TEST(NoteheadClassification, ClassifiesDiamondNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0DB); // noteheadDiamondBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Diamond);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0DD); // noteheadDiamondWhite
    EXPECT_EQ(unfilled.shape, notehead::Shape::Diamond);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesXNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A9); // noteheadXBlack
    EXPECT_EQ(filled.shape, notehead::Shape::X);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A8); // noteheadXHalf
    EXPECT_EQ(unfilled.shape, notehead::Shape::X);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesSlashNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto small = classifyNoteheadSymbol(fontContext.fontInfo, 0xE105); // noteheadSlashVerticalEndsSmall
    EXPECT_EQ(small.shape, notehead::Shape::SmallSlash);
    EXPECT_EQ(small.fill, notehead::Fill::Filled);

    auto large = classifyNoteheadSymbol(fontContext.fontInfo, 0xE100); // noteheadSlashVerticalEnds
    EXPECT_EQ(large.shape, notehead::Shape::LargeSlash);
    EXPECT_EQ(large.fill, notehead::Fill::Filled);

    auto largeUnfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE102); // noteheadSlashWhiteWhole
    EXPECT_EQ(largeUnfilled.shape, notehead::Shape::LargeSlash);
    EXPECT_EQ(largeUnfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesCircledNoteheadsBySmuflName)
{
    const auto fontContext = makeFontContext("Finale Maestro");

    auto filled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0E4); // noteheadCircledBlack
    EXPECT_EQ(filled.shape, notehead::Shape::Circled);
    EXPECT_EQ(filled.fill, notehead::Fill::Filled);

    auto unfilled = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0E6); // noteheadCircledWhole
    EXPECT_EQ(unfilled.shape, notehead::Shape::Circled);
    EXPECT_EQ(unfilled.fill, notehead::Fill::Unfilled);
}

TEST(NoteheadClassification, ClassifiesLegacyMaestroGlyphs)
{
    // Default font context is "Maestro" with the Mac symbol charset -- a legacy symbol font.
    const auto fontContext = makeFontContext();

    auto regular = classifyNoteheadSymbol(fontContext.fontInfo, 207); // -> noteheadBlack
    EXPECT_EQ(regular.shape, notehead::Shape::Regular);
    EXPECT_EQ(regular.fill, notehead::Fill::Filled);

    auto diamond = classifyNoteheadSymbol(fontContext.fontInfo, 226); // -> noteheadDiamondBlack
    EXPECT_EQ(diamond.shape, notehead::Shape::Diamond);
    EXPECT_EQ(diamond.fill, notehead::Fill::Filled);

    auto crossNotehead = classifyNoteheadSymbol(fontContext.fontInfo, 192); // -> noteheadXBlack
    EXPECT_EQ(crossNotehead.shape, notehead::Shape::X);
    EXPECT_EQ(crossNotehead.fill, notehead::Fill::Filled);
}

TEST(NoteheadClassification, ClassifiesAsciiXInNonSymbolFontOnly)
{
    const auto nonSymbolContext = makeFontContext("Times New Roman", /*charsetVal*/ 0);
    auto lower = classifyNoteheadSymbol(nonSymbolContext.fontInfo, U'x');
    EXPECT_EQ(lower.shape, notehead::Shape::X);
    EXPECT_EQ(lower.fill, notehead::Fill::Filled);
    EXPECT_FALSE(lower.glyphName);

    auto upper = classifyNoteheadSymbol(nonSymbolContext.fontInfo, U'X');
    EXPECT_EQ(upper.shape, notehead::Shape::X);

    // In a symbol font, codepoint 'X' (0x58) is a glyph index, not literal text -- must not match.
    // In Maestro it is the glyph note16thDown.
    const auto symbolContext = makeFontContext();
    auto notX = classifyNoteheadSymbol(symbolContext.fontInfo, U'X');
    EXPECT_NE(notX.shape, notehead::Shape::X);
    ASSERT_TRUE(notX.glyphName);
    EXPECT_EQ(notX.glyphName.value(), "note16thDown");
}

TEST(NoteheadClassification, ReturnsUnclassifiedForUnknownSymbol)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0x41); // 'A', not mapped
    EXPECT_FALSE(classification);
    EXPECT_EQ(classification.shape, notehead::Shape::Unclassified);
    EXPECT_EQ(classification.fill, notehead::Fill::Unspecified);
    // classifyNoteheadSymbol has no note to resolve, so the source is the document default.
    EXPECT_EQ(classification.noteheadInfo.source, NoteInfoPtr::NoteheadInfo::Source::Default);
    EXPECT_FALSE(classification.calcOverridesDefault(NoteType::Quarter));

    // The same unrecognized glyph counts as an override once a customization supplied it.
    classification.noteheadInfo.source = NoteInfoPtr::NoteheadInfo::Source::NoteAlteration;
    EXPECT_TRUE(classification.calcOverridesDefault(NoteType::Quarter));
    classification.noteheadInfo.source = NoteInfoPtr::NoteheadInfo::Source::ShapeNotes;
    EXPECT_TRUE(classification.calcOverridesDefault(NoteType::Quarter));
}

TEST(NoteheadClassification, ReturnsOtherForRecognizedButUncatalogedNoteheadGlyph)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    // noteheadTriangleUpRightBlack is a real SMuFL notehead glyph, but not one of the shapes Denigma
    // specifically classifies.
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0C9);
    EXPECT_TRUE(classification);
    EXPECT_EQ(classification.shape, notehead::Shape::Other);
    EXPECT_EQ(classification.fill, notehead::Fill::Unspecified);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "noteheadTriangleUpRightBlack");
}

TEST(NoteheadClassification, ClassifyNoteheadSymbolPopulatesNoteheadInfo)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    auto classification = classifyNoteheadSymbol(fontContext.fontInfo, 0xE0A4); // noteheadBlack
    EXPECT_EQ(classification.noteheadInfo.font, fontContext.fontInfo);
    EXPECT_EQ(classification.noteheadInfo.character, char32_t(0xE0A4));
    EXPECT_EQ(classification.noteheadInfo.percent, 100);
    EXPECT_EQ(classification.noteheadInfo.horzOffset, 0);
    EXPECT_EQ(classification.noteheadInfo.vertOffset, 0);
}

TEST(NoteheadClassification, ClassifyNoteheadReturnsUnclassifiedForInvalidNote)
{
    EXPECT_FALSE(classifyNotehead(NoteInfoPtr()));
}

TEST(PercussionNoteheadClassification, FamilyFollowsQuarterNoteheadDespiteLongerShapes)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    // Finale's stock drum map draws its X noteheads as circled X noteheads on half notes and longer.
    const auto xNote = findPercussionNote(document, 0xE0A9); // noteheadXBlack
    ASSERT_TRUE(xNote);
    const auto classification = classifyPercussionNotehead(xNote);
    ASSERT_TRUE(classification);
    EXPECT_EQ(classification.family, notehead::Shape::X);
    EXPECT_EQ(classification.half.shape, notehead::Shape::CircleX);
    EXPECT_EQ(classification.whole.shape, notehead::Shape::CircleX);
    EXPECT_EQ(classification.breveAndLonger.shape, notehead::Shape::CircleX);
    EXPECT_EQ(classification.quarterAndShorter.noteheadInfo.source, NoteInfoPtr::NoteheadInfo::Source::PercussionMap);
    EXPECT_FALSE(classification.quarterAndShorter.calcDiffersFromFamily(classification.family, NoteType::Eighth));
    EXPECT_TRUE(classification.half.calcDiffersFromFamily(classification.family, NoteType::Half));
    EXPECT_TRUE(classification.whole.calcDiffersFromFamily(classification.family, NoteType::Whole));
}

TEST(PercussionNoteheadClassification, ConsistentFamiliesConformAtEveryDuration)
{
    const auto document = loadFixture("drumset.musx");
    ASSERT_TRUE(document);

    const auto check = [&](char32_t closedNotehead, notehead::Shape expectedFamily) {
        const auto percussionNote = findPercussionNote(document, closedNotehead);
        ASSERT_TRUE(percussionNote);
        const auto classification = classifyPercussionNotehead(percussionNote);
        EXPECT_EQ(classification.family, expectedFamily);
        EXPECT_FALSE(classification.quarterAndShorter.calcDiffersFromFamily(expectedFamily, NoteType::Quarter));
        EXPECT_FALSE(classification.half.calcDiffersFromFamily(expectedFamily, NoteType::Half));
        EXPECT_FALSE(classification.whole.calcDiffersFromFamily(expectedFamily, NoteType::Whole));
        EXPECT_FALSE(classification.breveAndLonger.calcDiffersFromFamily(expectedFamily, NoteType::Breve));
    };
    check(0xE0A4, notehead::Shape::Regular);  // noteheadBlack
    check(0xE0BE, notehead::Shape::Triangle); // noteheadTriangleUpBlack
}

TEST(PercussionNoteheadClassification, FamilyIsUnclassifiedOnlyWhenQuarterNoteheadIs)
{
    const auto fontContext = makeFontContext();
    auto percussionNote = std::make_shared<others::PercussionNoteInfo>(fontContext.document, SCORE_PARTID, EnigmaBase::ShareMode::All, 1, 0);

    // A blank quarter notehead is a null notehead, whatever the longer ones are.
    percussionNote->closedNotehead = U' ';
    percussionNote->halfNotehead = 0xE0A3; // noteheadHalf
    EXPECT_EQ(classifyPercussionNotehead(percussionNote).family, notehead::Shape::Null);

    // A quarter notehead that names no glyph leaves the family unclassified.
    percussionNote->closedNotehead = U'q';
    const auto classification = classifyPercussionNotehead(percussionNote);
    EXPECT_FALSE(classification);
    EXPECT_EQ(classification.family, notehead::Shape::Unclassified);

    EXPECT_FALSE(classifyPercussionNotehead(nullptr));
}

TEST(NoteheadClassification, ClassifiesPercussionShapeFamilies)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    const auto expectShape = [&](char32_t symbol, notehead::Shape shape, notehead::Fill fill) {
        const auto classification = classifyNoteheadSymbol(fontContext.fontInfo, symbol);
        EXPECT_EQ(classification.shape, shape) << std::hex << uint32_t(symbol);
        EXPECT_EQ(classification.fill, fill) << std::hex << uint32_t(symbol);
    };
    expectShape(0xE0B3, notehead::Shape::CircleX, notehead::Fill::Unspecified);   // noteheadCircleX
    expectShape(0xE0B2, notehead::Shape::CircleX, notehead::Fill::Unspecified);   // noteheadCircleXHalf
    expectShape(0xE0AF, notehead::Shape::Plus, notehead::Fill::Unspecified);      // noteheadPlusBlack
    expectShape(0xE0AE, notehead::Shape::Plus, notehead::Fill::Unspecified);      // noteheadPlusHalf
    expectShape(0xE0BE, notehead::Shape::Triangle, notehead::Fill::Filled);       // noteheadTriangleUpBlack
    expectShape(0xE0BC, notehead::Shape::Triangle, notehead::Fill::Unfilled);     // noteheadTriangleUpHalf
    expectShape(0xE0C7, notehead::Shape::TriangleDown, notehead::Fill::Filled);   // noteheadTriangleDownBlack
    expectShape(0xE0C5, notehead::Shape::TriangleDown, notehead::Fill::Unfilled); // noteheadTriangleDownHalf
    expectShape(0xE0B9, notehead::Shape::Square, notehead::Fill::Filled);         // noteheadSquareBlack
    expectShape(0xE0B8, notehead::Shape::Square, notehead::Fill::Unfilled);       // noteheadSquareWhite
}

TEST(NoteheadClassification, ClassifiesAnyNamedGlyphAsOther)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    // Shape-note and note-name noteheads are named outside SMuFL's notehead glyphs, and a notehead may be any
    // glyph at all. Each is still a notehead whose glyph is known.
    const auto expectOther = [&](char32_t symbol, std::string_view glyphName) {
        const auto classification = classifyNoteheadSymbol(fontContext.fontInfo, symbol);
        EXPECT_EQ(classification.shape, notehead::Shape::Other) << glyphName;
        ASSERT_TRUE(classification.glyphName) << glyphName;
        EXPECT_EQ(classification.glyphName.value(), glyphName);
    };
    expectOther(0xE1B1, "noteShapeRoundBlack");
    expectOther(0xE150, "noteDoWhole");
    expectOther(0xE050, "gClef");
}

TEST(NoteheadClassification, ClassifiesSymbolFontCharactersStoredWithWindowsOffset)
{
    // termolos_twonotes.musx overrides noteheads with Maestro characters stored as U+F077 and U+F0FA,
    // the Windows form of the whole and half noteheads.
    const auto document = loadFixture("termolos_twonotes.musx");
    ASSERT_TRUE(document);
    size_t overrideCount = 0;
    document->iterateEntries(SCORE_PARTID, [&](const EntryInfoPtr& entryInfo) -> bool {
        for (size_t x = 0; x < entryInfo->getEntry()->notes.size(); x++) {
            const auto classification = classifyNotehead(NoteInfoPtr(entryInfo, x));
            if (classification.noteheadInfo.source != NoteInfoPtr::NoteheadInfo::Source::NoteAlteration) {
                continue;
            }
            ++overrideCount;
            EXPECT_EQ(classification.shape, notehead::Shape::Regular);
            const auto glyphName = classification.glyphName.value_or("");
            EXPECT_TRUE(glyphName == "noteheadHalf" || glyphName == "noteheadWhole") << glyphName;
        }
        return true;
    });
    EXPECT_GT(overrideCount, 0u);
}
