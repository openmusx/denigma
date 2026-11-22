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

#include <algorithm>
#include <filesystem>
#include <memory>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/articulations.h"
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

static void expectNoteSlide(const musx::dom::MusxInstance<FontInfo>& fontInfo, char32_t symbol, articulation::NoteSlide::Type expectedType,
    bool expectedIntoNote, const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* slide = classification.as<articulation::NoteSlide>();
    ASSERT_NE(slide, nullptr);
    EXPECT_EQ(slide->type, expectedType);
    EXPECT_EQ(slide->isIntoNote(), expectedIntoNote);
    // A glyph alone names no entry, so it names no note either.
    EXPECT_FALSE(slide->note);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectSingleTechniqueMark(const musx::dom::MusxInstance<FontInfo>& fontInfo, char32_t symbol,
    articulation::TechniqueMark::Type expectedType, const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* technique = classification.as<articulation::TechniqueMark>();
    ASSERT_NE(technique, nullptr);
    EXPECT_EQ(technique->type, expectedType);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectHarmonMute(const musx::dom::MusxInstance<FontInfo>& fontInfo, char32_t symbol,
    articulation::HarmonMute::Qualifier expectedQualifier, const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* harmonMute = classification.as<articulation::HarmonMute>();
    ASSERT_NE(harmonMute, nullptr);
    EXPECT_EQ(harmonMute->qualifier, expectedQualifier);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectPluckedDamp(const musx::dom::MusxInstance<FontInfo>& fontInfo, char32_t symbol, articulation::PluckedDamp::Type expectedType,
    const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* pluckedDamp = classification.as<articulation::PluckedDamp>();
    ASSERT_NE(pluckedDamp, nullptr);
    EXPECT_EQ(pluckedDamp->type, expectedType);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

static void expectStringMute(const musx::dom::MusxInstance<FontInfo>& fontInfo, char32_t symbol, articulation::StringMute::Type expectedType,
    const std::string& expectedGlyphName)
{
    const auto classification = classifyArticulationSymbol(fontInfo, symbol);
    const auto* stringMute = classification.as<articulation::StringMute>();
    ASSERT_NE(stringMute, nullptr);
    EXPECT_EQ(stringMute->type, expectedType);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
}

} // namespace

TEST(ArticulationClassification, ClassifiesUnicodeArticulationMarks)
{
    const auto fontContext = makeFontContext("Times New Roman", 0);
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 0x1D17B);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, articulation::ArticulationMark::Type::Accent);
    EXPECT_FALSE(classification.glyphName);
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, GlyphStyle::Placement::Automatic);
}

TEST(ArticulationClassification, ClassifiesAccordionRegistrationGlyph)
{
    const auto context = makeFontContext("Bravura", 4095);
    const auto result = classifyArticulationSymbol(context.fontInfo, U'\uE8A1');

    const auto* accordion = result.as<articulation::AccordionRegistration>();
    ASSERT_NE(accordion, nullptr);
    ASSERT_EQ(accordion->glyphNames.size(), 1u);
    EXPECT_EQ(accordion->glyphNames.front(), "accdnRH3RanksClarinet");
    EXPECT_EQ(accordion->hand, articulation::AccordionRegistration::Hand::Right);
    EXPECT_EQ(accordion->rankCount, articulation::AccordionRegistration::RankCount::Three);
    EXPECT_EQ(accordion->instrumentType, articulation::AccordionRegistration::InstrumentType::Clarinet);
    ASSERT_EQ(accordion->dots.size(), 1u);
    EXPECT_EQ(accordion->dots.front().position, articulation::AccordionRegistration::DotPosition::Middle);
    ASSERT_TRUE(result.glyphName.has_value());
    EXPECT_EQ(*result.glyphName, "accdnRH3RanksClarinet");
}

TEST(ArticulationClassification, ClassifiesLegacyGlyphArticulationMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 62);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 1u);
    EXPECT_EQ(articulation->marks.front().type, articulation::ArticulationMark::Type::Accent);
    ASSERT_TRUE(classification.glyphName);
    EXPECT_EQ(classification.glyphName.value(), "articAccentAbove");
    EXPECT_EQ(articulation->marks.front().glyphStyle.placement, GlyphStyle::Placement::Above);
}

TEST(ArticulationClassification, AssignsGlyphStyleToEachComboArticulationMark)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 223);

    const auto* articulation = classification.as<articulation::ArticulationMarks>();
    ASSERT_NE(articulation, nullptr);
    ASSERT_EQ(articulation->marks.size(), 2u);
    EXPECT_EQ(articulation->marks[0].type, articulation::ArticulationMark::Type::Accent);
    EXPECT_EQ(articulation->marks[0].glyphStyle.placement, GlyphStyle::Placement::Below);
    EXPECT_EQ(articulation->marks[1].type, articulation::ArticulationMark::Type::Staccato);
    EXPECT_EQ(articulation->marks[1].glyphStyle.placement, GlyphStyle::Placement::Below);
}

TEST(ArticulationClassification, ClassifiesTechniqueGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE632, articulation::TechniqueMark::Type::BuzzPizzicato, "pluckedBuzzPizzicato");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE631, articulation::TechniqueMark::Type::SnapPizzicato, "pluckedSnapPizzicatoAbove");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xF432, articulation::TechniqueMark::Type::SnapPizzicato, "pluckedSnapPizzicatoBelowGerman");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE636, articulation::TechniqueMark::Type::Fingernails, "pluckedWithFingernails");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E1, articulation::TechniqueMark::Type::BrassFlip, "brassFlip");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E6, articulation::TechniqueMark::Type::BrassHalfMuted, "brassMuteHalfClosed");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E2, articulation::TechniqueMark::Type::BrassSmear, "brassSmear");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE5E3, articulation::TechniqueMark::Type::BrassBend, "brassBend");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xF767, articulation::TechniqueMark::Type::BrassLift, "brassLiftSlight");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE624, articulation::TechniqueMark::Type::ThumbPosition, "stringsThumbPosition");
    expectSingleTechniqueMark(fontContext.fontInfo, 0xE625, articulation::TechniqueMark::Type::ThumbPosition, "stringsThumbPositionTurned");
}

TEST(ArticulationClassification, ClassifiesHarmonMuteGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectHarmonMute(fontContext.fontInfo, 0xE5E8, articulation::HarmonMute::Qualifier::Closed, "brassHarmonMuteClosed");
    expectHarmonMute(fontContext.fontInfo, 0xE5E9, articulation::HarmonMute::Qualifier::HalfLeft, "brassHarmonMuteStemHalfLeft");
    expectHarmonMute(fontContext.fontInfo, 0xE5EA, articulation::HarmonMute::Qualifier::HalfRight, "brassHarmonMuteStemHalfRight");
    expectHarmonMute(fontContext.fontInfo, 0xE5EB, articulation::HarmonMute::Qualifier::Open, "brassHarmonMuteStemOpen");
}

TEST(ArticulationClassification, ClassifiesPluckedDampGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectPluckedDamp(fontContext.fontInfo, 0xE638, articulation::PluckedDamp::Type::Damp, "pluckedDamp");
    expectPluckedDamp(fontContext.fontInfo, 0xE639, articulation::PluckedDamp::Type::DampAll, "pluckedDampAll");
}

TEST(ArticulationClassification, ClassifiesStringMuteGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectStringMute(fontContext.fontInfo, 0xE616, articulation::StringMute::Type::On, "stringsMuteOn");
    expectStringMute(fontContext.fontInfo, 0xE617, articulation::StringMute::Type::Off, "stringsMuteOff");
}

TEST(ArticulationClassification, ClassifiesBrassArticulationGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    expectNoteSlide(fontContext.fontInfo, 0xE5D0, articulation::NoteSlide::Type::Scoop, true, "brassScoop");
    expectNoteSlide(fontContext.fontInfo, 0xE5D4, articulation::NoteSlide::Type::Doit, false, "brassDoitShort");
    expectNoteSlide(fontContext.fontInfo, 0xE5D7, articulation::NoteSlide::Type::Falloff, false, "brassFallLipShort");
    expectNoteSlide(fontContext.fontInfo, 0xE5E0, articulation::NoteSlide::Type::Plop, true, "brassPlop");
    expectNoteSlide(fontContext.fontInfo, 0xF768, articulation::NoteSlide::Type::Falloff, false, "brassFallSlight");
}

TEST(ArticulationClassification, ClassifiesFermatasBreathMarksAndArpeggios)
{
    const auto fontContext = makeFontContext();
    const auto fermataClassification = classifyArticulationSymbol(fontContext.fontInfo, 85);
    const auto* fermata = fermataClassification.as<articulation::Fermata>();
    ASSERT_NE(fermata, nullptr);
    EXPECT_EQ(fermata->shape, articulation::Fermata::Shape::Normal);
    EXPECT_EQ(fermata->duration, articulation::Fermata::Duration::Auto);

    const auto breathClassification = classifyArticulationSymbol(fontContext.fontInfo, 44);
    const auto* breath = breathClassification.as<articulation::BreathMark>();
    ASSERT_NE(breath, nullptr);
    EXPECT_EQ(breath->type, articulation::BreathMark::Type::Comma);

    const auto arpeggioClassification = classifyArticulationSymbol(fontContext.fontInfo, 103);
    const auto* arpeggio = arpeggioClassification.as<articulation::Arpeggio>();
    ASSERT_NE(arpeggio, nullptr);
    EXPECT_EQ(arpeggio->type, articulation::Arpeggio::Type::VerticalSegment);
}

TEST(ArticulationClassification, ClassifiesCaesuras)
{
    const auto fontContext = makeFontContext();
    const auto normalClassification = classifyArticulationSymbol(fontContext.fontInfo, 34);
    const auto* normal = normalClassification.as<articulation::Caesura>();
    ASSERT_NE(normal, nullptr);
    EXPECT_EQ(normal->type, articulation::Caesura::Type::Normal);

    const auto chantContext = makeFontContext("Finale Maestro");
    const auto chantClassification = classifyArticulationSymbol(chantContext.fontInfo, 0xE8F8);
    const auto* chant = chantClassification.as<articulation::Caesura>();
    ASSERT_NE(chant, nullptr);
    EXPECT_EQ(chant->type, articulation::Caesura::Type::Chant);
}

TEST(ArticulationClassification, ClassifiesOrnaments)
{
    const auto fontContext = makeFontContext();
    const auto trillClassification = classifyArticulationSymbol(fontContext.fontInfo, 217);
    const auto* trill = trillClassification.as<articulation::Ornament>();
    ASSERT_NE(trill, nullptr);
    EXPECT_EQ(trill->type, articulation::Ornament::Type::Trill);
    EXPECT_TRUE(trill->accidentals.empty());

    const auto mordentClassification = classifyArticulationSymbol(fontContext.fontInfo, 77);
    const auto* mordent = mordentClassification.as<articulation::Ornament>();
    ASSERT_NE(mordent, nullptr);
    EXPECT_EQ(mordent->type, articulation::Ornament::Type::Mordent);

    const auto invertedTurnContext = makeFontContext("Kousaku");
    const auto invertedTurnClassification = classifyArticulationSymbol(invertedTurnContext.fontInfo, 255);
    const auto* invertedTurn = invertedTurnClassification.as<articulation::Ornament>();
    ASSERT_NE(invertedTurn, nullptr);
    EXPECT_EQ(invertedTurn->type, articulation::Ornament::Type::InvertedTurn);

    const auto trillFlatContext = makeFontContext("Finale Maestro");
    const auto trillFlatClassification = classifyArticulationSymbol(trillFlatContext.fontInfo, 0xF5B2);
    const auto* trillFlat = trillFlatClassification.as<articulation::Ornament>();
    ASSERT_NE(trillFlat, nullptr);
    EXPECT_EQ(trillFlat->type, articulation::Ornament::Type::Trill);
    ASSERT_EQ(trillFlat->accidentals.size(), 1u);
    EXPECT_EQ(trillFlat->accidentals.front().accidental, articulation::Ornament::Accidental::Flat);
    EXPECT_EQ(trillFlat->accidentals.front().placement, musx::dom::VerticalPlacement::Above);

    const auto turnFlatSharpContext = makeFontContext("Finale Maestro");
    const auto turnFlatSharpClassification = classifyArticulationSymbol(turnFlatSharpContext.fontInfo, 0xF5B6);
    const auto* turnFlatSharp = turnFlatSharpClassification.as<articulation::Ornament>();
    ASSERT_NE(turnFlatSharp, nullptr);
    EXPECT_EQ(turnFlatSharp->type, articulation::Ornament::Type::Turn);
    ASSERT_EQ(turnFlatSharp->accidentals.size(), 2u);
    EXPECT_EQ(turnFlatSharp->accidentals[0].accidental, articulation::Ornament::Accidental::Flat);
    EXPECT_EQ(turnFlatSharp->accidentals[0].placement, musx::dom::VerticalPlacement::Above);
    EXPECT_EQ(turnFlatSharp->accidentals[1].accidental, articulation::Ornament::Accidental::Sharp);
    EXPECT_EQ(turnFlatSharp->accidentals[1].placement, musx::dom::VerticalPlacement::Below);
}

TEST(ArticulationClassification, ClassifiesTremoloMarks)
{
    const auto fontContext = makeFontContext();
    const auto classification = classifyArticulationSymbol(fontContext.fontInfo, 190);

    const auto* tremolo = classification.as<articulation::Tremolo>();
    ASSERT_NE(tremolo, nullptr);
    EXPECT_EQ(tremolo->style, articulation::Tremolo::Style::Measured);
    EXPECT_EQ(tremolo->marks, 3);
}

TEST(ArticulationClassification, ClassifiesUnmeasuredTremoloGlyphs)
{
    const auto fontContext = makeFontContext("Finale Maestro");
    const auto expectUnmeasuredTremolo = [&](char32_t symbol, const std::string& expectedGlyphName) {
        const auto classification = classifyArticulationSymbol(fontContext.fontInfo, symbol);
        const auto* tremolo = classification.as<articulation::Tremolo>();
        ASSERT_NE(tremolo, nullptr);
        EXPECT_EQ(tremolo->style, articulation::Tremolo::Style::Unmeasured);
        EXPECT_EQ(tremolo->marks, 0);
        ASSERT_TRUE(classification.glyphName);
        EXPECT_EQ(classification.glyphName.value(), expectedGlyphName);
    };

    expectUnmeasuredTremolo(0xE213, "stemPendereckiTremolo");
    expectUnmeasuredTremolo(0xE22A, "buzzRoll");
    expectUnmeasuredTremolo(0xE22B, "pendereckiTremolo");
    expectUnmeasuredTremolo(0xE22C, "unmeasuredTremolo");
    expectUnmeasuredTremolo(0xE22D, "unmeasuredTremoloSimple");
    expectUnmeasuredTremolo(0xE232, "stockhausenTremolo");
}

TEST(ArticulationClassification, LeavesAsciiParenthesesUnclassified)
{
    const auto fontContext = makeFontContext("Times New Roman", 0);
    EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, U'('));
    EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, U')'));
}

TEST(ArticulationClassification, LeavesLegacyParenthesisGlyphsUnclassified)
{
    const auto fontContext = makeFontContext();
    EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, U'('));
    EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, U')'));
}

TEST(ArticulationClassification, LeavesSmuflParenthesisGlyphsUnclassified)
{
    constexpr char32_t leftParenthesis = U'\uE0F5';
    constexpr char32_t rightParenthesis = U'\uE0F6';
    for (const auto* fontName : {"Finale Maestro", "Times New Roman"}) {
        const auto fontContext = makeFontContext(fontName, 0);
        EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, leftParenthesis)) << fontName;
        EXPECT_FALSE(classifyArticulationSymbol(fontContext.fontInfo, rightParenthesis)) << fontName;
    }
}

TEST(ArticulationClassification, ClassifiesVerticalEntryBracketShapes)
{
    std::vector<char> xml;
    readFile(std::filesystem::path(MUSX_TEST_DATA_PATH) / "nonArpeggios.enigmaxml", xml);
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);

    auto sourceEntry = musx::dom::EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 131);
    ASSERT_TRUE(sourceEntry);
    auto assign = document->getDetails()->get<details::ArticulationAssign>(SCORE_PARTID, 131, Inci{0});
    ASSERT_TRUE(assign);
    const auto classifications = classifyArticulations(sourceEntry);
    ASSERT_FALSE(classifications.empty());
    const auto& classification = classifications.front();
    EXPECT_EQ(classification.assignment->getInci(), assign->getInci());
    EXPECT_TRUE(classification.is<articulation::VerticalEntryBracket>());
    EXPECT_NE(classification.placement, musx::dom::VerticalPlacement::NotApplicable);
}

TEST(ArticulationClassification, ClassifiesPseudoLaissezVibrerShapeArticulations)
{
    std::vector<char> xml;
    readFile(std::filesystem::path(MUSX_TEST_DATA_PATH) / "lvshapes.enigmaxml", xml);
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);

    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 23);
    ASSERT_TRUE(entryInfo);
    const auto assignments = document->getDetails()->getArray<details::ArticulationAssign>(SCORE_PARTID, 23);
    ASSERT_EQ(assignments.size(), 2u);
    const auto classifications = classifyArticulations(entryInfo);
    ASSERT_EQ(classifications.size(), assignments.size());
    for (const auto& classification : classifications) {
        const auto* pseudoTie = classification.as<PseudoTie>();
        ASSERT_NE(pseudoTie, nullptr);
        EXPECT_EQ(pseudoTie->type, PseudoTie::Type::LaissezVibrer);
    }
}

namespace {

/// Loads notehead_enclosures.enigmaxml, derived from timesigs_independent.musx. Each case is one entry with delimiter
/// articulations offset relative to what they may enclose, mostly quarter notes with manually positioned delimiters.
/// Other cases cover chords with a second, grace notes, delimiters centered by Finale, whole and double whole notes,
/// hidden delimiters, and rests.
DocumentPtr loadEnclosureFixture()
{
    setupTestDataPaths();
    std::vector<char> xml;
    readFile(getInputPath() / "notehead_enclosures.enigmaxml", xml);
    return musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
}

/// Classifies the entry of the fixture's 1-based case number.
std::vector<ArticulationClassification> classifyFixtureCase(const DocumentPtr& document, EntryNumber caseNumber)
{
    constexpr EntryNumber FIRST_CASE_ENTRY = 60;
    const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, FIRST_CASE_ENTRY + caseNumber - 1);
    EXPECT_TRUE(entryInfo) << "case " << caseNumber;
    return entryInfo ? classifyArticulations(entryInfo) : std::vector<ArticulationClassification>{};
}

size_t countEnclosures(const std::vector<ArticulationClassification>& classifications)
{
    return static_cast<size_t>(std::count_if(classifications.begin(), classifications.end(),
        [](const ArticulationClassification& classification) { return classification.is<articulation::BracketEnclosure>(); }));
}

} // namespace

TEST(ArticulationClassification, ClassifiesEnclosures)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    using Type = articulation::BracketEnclosure::Type;
    struct Expected
    {
        EntryNumber caseNumber;
        Type type;
        const char* glyphName;
        const char* description;
    };
    // clang-format off
    const std::vector<Expected> expectedEnclosures = {
        { 1,  Type::Parentheses, "noteheadParenthesisLeft", "legacy Maestro parentheses" },
        { 2,  Type::Parentheses, "noteheadParenthesisLeft", "closing delimiter at the vertical tolerance from the opening" },
        { 4,  Type::Parentheses, "noteheadParenthesisLeft", "at the bottom of the note's window" },
        { 6,  Type::Parentheses, "noteheadParenthesisLeft", "just under the top of the note's window" },
        { 11, Type::Brackets,    nullptr,                   "ASCII brackets in a text font" },
        { 16, Type::Parentheses, "noteheadParenthesisLeft", "SMuFL notehead parentheses" },
        { 17, Type::Parentheses, "accidentalParensLeft",    "SMuFL accidental parentheses" },
        { 20, Type::Parentheses, "noteheadParenthesisLeft", "grace note" },
        { 21, Type::Parentheses, "noteheadParenthesisLeft", "closing delimiter right of a grace-size notehead's center" },
        { 24, Type::Parentheses, "noteheadParenthesisLeft", "delimiters centered on the notehead and offset" },
        { 28, Type::Parentheses, "noteheadParenthesisLeft", "closing delimiter right of a whole notehead's center" },
        { 30, Type::Parentheses, "noteheadParenthesisLeft", "closing delimiter right of a double whole notehead's center" },
    };
    // clang-format on
    for (const auto& expected : expectedEnclosures) {
        const auto classifications = classifyFixtureCase(document, expected.caseNumber);
        ASSERT_EQ(classifications.size(), 1u) << expected.description;
        const auto& classification = classifications.front();
        const auto* enclosure = classification.as<articulation::BracketEnclosure>();
        ASSERT_NE(enclosure, nullptr) << expected.description;
        EXPECT_EQ(enclosure->type, expected.type) << expected.description;
        ASSERT_TRUE(enclosure->note) << expected.description;
        EXPECT_EQ(enclosure->note.getNoteIndex(), 0u) << expected.description;
        EXPECT_EQ(classification.assignment->getInci(), Inci{0}) << expected.description;
        ASSERT_TRUE(enclosure->closingAssignment) << expected.description;
        EXPECT_EQ(enclosure->closingAssignment->getInci(), Inci{1}) << expected.description;
        if (expected.glyphName) {
            EXPECT_EQ(classification.glyphName, std::optional<std::string>(expected.glyphName)) << expected.description;
        } else {
            EXPECT_FALSE(classification.glyphName) << expected.description;
        }
    }
}

TEST(ArticulationClassification, ClassifiesRestEnclosures)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    using Type = articulation::BracketEnclosure::Type;
    struct Expected
    {
        EntryNumber caseNumber;
        Type type;
        const char* description;
    };
    // clang-format off
    const std::vector<Expected> expectedEnclosures = {
        { 35, Type::Parentheses, "parentheses around a floating rest" },
        { 36, Type::Parentheses, "parentheses around a positioned rest" },
        { 37, Type::Brackets,    "brackets around a rest" },
        { 38, Type::Parentheses, "at the bottom of the rest's window" },
        { 40, Type::Parentheses, "just under the top of the rest's window" },
        { 43, Type::Parentheses, "delimiters centered on the rest and offset" },
    };
    // clang-format on
    for (const auto& expected : expectedEnclosures) {
        const auto classifications = classifyFixtureCase(document, expected.caseNumber);
        ASSERT_EQ(classifications.size(), 1u) << expected.description;
        const auto* enclosure = classifications.front().as<articulation::BracketEnclosure>();
        ASSERT_NE(enclosure, nullptr) << expected.description;
        EXPECT_EQ(enclosure->type, expected.type) << expected.description;
        // A rest is enclosed as a whole entry.
        EXPECT_FALSE(enclosure->note) << expected.description;
        ASSERT_TRUE(enclosure->closingAssignment) << expected.description;
    }
}

TEST(ArticulationClassification, LeavesUnpairedNoteheadDelimitersUnclassified)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    struct Expected
    {
        EntryNumber caseNumber;
        size_t assignments;
        const char* description;
    };
    // clang-format off
    const std::vector<Expected> expectedNegatives = {
        { 3,  2, "closing delimiter just beyond the vertical tolerance from the opening" },
        { 5,  2, "below the note's range" },
        { 7,  2, "at the top of the note's window" },
        { 8,  2, "opening delimiter not left of the entry" },
        { 9,  2, "closing delimiter not right of the notehead's center" },
        { 10, 1, "lone opening delimiter" },
        { 12, 2, "delimiters in different fonts" },
        { 13, 2, "hidden closing delimiter" },
        { 14, 2, "automatically positioned opening delimiter" },
        { 18, 2, "a bracket closed by a parenthesis" },
        { 22, 2, "closing delimiter not right of a grace-size notehead's center" },
        { 25, 2, "centered opening delimiter not left of the entry" },
        { 29, 2, "closing delimiter not right of a whole notehead's center" },
        { 31, 2, "closing delimiter not right of a double whole notehead's center" },
        { 39, 2, "below the rest's window" },
        { 41, 2, "at the top of the rest's window" },
        { 42, 2, "closing delimiter not right of the rest's center" },
    };
    // clang-format on
    for (const auto& expected : expectedNegatives) {
        const auto classifications = classifyFixtureCase(document, expected.caseNumber);
        ASSERT_EQ(classifications.size(), expected.assignments) << expected.description;
        for (const auto& classification : classifications) {
            EXPECT_FALSE(classification) << expected.description;
        }
    }
}

TEST(ArticulationClassification, ReturnsHiddenAssignments)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // Only the closing delimiter is hidden, so the pair does not enclose the note and both are returned unpaired.
    const auto classifications = classifyFixtureCase(document, 13);
    ASSERT_EQ(classifications.size(), 2u);
    EXPECT_FALSE(classifications[0].assignment->hide);
    ASSERT_TRUE(classifications[1].assignment);
    EXPECT_TRUE(classifications[1].assignment->hide);
    EXPECT_EQ(classifications[1].assignment->getInci(), Inci{1});
}

TEST(ArticulationClassification, PairsHiddenNoteheadDelimiters)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // Both delimiters are hidden, so they pair, and the caller decides what to do with a hidden enclosure.
    const auto classifications = classifyFixtureCase(document, 34);
    ASSERT_EQ(classifications.size(), 1u);
    ASSERT_TRUE(classifications.front().is<articulation::BracketEnclosure>());
    EXPECT_TRUE(classifications.front().assignment->hide);
    EXPECT_TRUE(classifications.front().as<articulation::BracketEnclosure>()->closingAssignment->hide);
}

TEST(ArticulationClassification, PairsNoteheadDelimitersPerChordNote)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // A chord with a second: one pair at each note.
    const auto classifications = classifyFixtureCase(document, 15);
    ASSERT_EQ(classifications.size(), 2u);
    ASSERT_EQ(countEnclosures(classifications), 2u);
    const auto* upper = classifications[0].as<articulation::BracketEnclosure>();
    const auto* lower = classifications[1].as<articulation::BracketEnclosure>();
    EXPECT_EQ(upper->note.getNoteIndex(), 1u);
    EXPECT_EQ(lower->note.getNoteIndex(), 0u);
    EXPECT_EQ(upper->closingAssignment->getInci(), Inci{1});
    EXPECT_EQ(lower->closingAssignment->getInci(), Inci{3});
}

TEST(ArticulationClassification, EnclosesClosestChordNote)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // A chord with a second, with the handle just below the lower note and so within the upper note's window too.
    const auto classifications = classifyFixtureCase(document, 23);
    ASSERT_EQ(classifications.size(), 1u);
    const auto* enclosure = classifications.front().as<articulation::BracketEnclosure>();
    ASSERT_NE(enclosure, nullptr);
    EXPECT_EQ(enclosure->note.getNoteIndex(), 0u);
}

TEST(ArticulationClassification, EnclosesDisplacedSecondFromPrimaryColumn)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // Both cases are down-stem chords with a second, whose lower note is drawn one notehead width left of the
    // primary column.
    for (const EntryNumber caseNumber : {26, 27}) {
        const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 60 + caseNumber - 1);
        ASSERT_TRUE(entryInfo);
        ASSERT_FALSE(entryInfo.calcUpStem()) << "case " << caseNumber;
    }
    // Centered delimiters offset to enclose the displaced lower note.
    const auto displaced = classifyFixtureCase(document, 26);
    ASSERT_EQ(displaced.size(), 1u);
    const auto* enclosure = displaced.front().as<articulation::BracketEnclosure>();
    ASSERT_NE(enclosure, nullptr);
    EXPECT_EQ(enclosure->note.getNoteIndex(), 0u);
    // Centered delimiters offset as for a note in the primary column do not enclose the displaced note.
    EXPECT_EQ(countEnclosures(classifyFixtureCase(document, 27)), 0u);
}

TEST(ArticulationClassification, EnclosesDisplacedSecondFromManualPlacement)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // Both cases are down-stem chords with a second, whose lower note is drawn one notehead width left of the
    // primary column.
    for (const EntryNumber caseNumber : {32, 33}) {
        const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 60 + caseNumber - 1);
        ASSERT_TRUE(entryInfo);
        ASSERT_FALSE(entryInfo.calcUpStem()) << "case " << caseNumber;
    }
    // Manually positioned delimiters placed around the displaced lower note.
    const auto displaced = classifyFixtureCase(document, 32);
    ASSERT_EQ(displaced.size(), 1u);
    const auto* enclosure = displaced.front().as<articulation::BracketEnclosure>();
    ASSERT_NE(enclosure, nullptr);
    EXPECT_EQ(enclosure->note.getNoteIndex(), 0u);
    // Manually positioned delimiters placed as for a note in the primary column do not enclose the displaced note.
    EXPECT_EQ(countEnclosures(classifyFixtureCase(document, 33)), 0u);
}

TEST(ArticulationClassification, ReportsEnclosureAtOpeningDelimiter)
{
    const auto document = loadEnclosureFixture();
    ASSERT_TRUE(document);
    // The closing delimiter is assigned before the opening one.
    const auto classifications = classifyFixtureCase(document, 19);
    ASSERT_EQ(classifications.size(), 1u);
    const auto* enclosure = classifications.front().as<articulation::BracketEnclosure>();
    ASSERT_NE(enclosure, nullptr);
    EXPECT_EQ(classifications.front().assignment->getInci(), Inci{1});
    EXPECT_EQ(enclosure->closingAssignment->getInci(), Inci{0});
}

TEST(ArticulationClassification, ClassifiesFinaleAuthoredEnclosures)
{
    setupTestDataPaths();
    const auto inputPath = getInputPath() / "note_enclosures.musx";
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    const auto document = denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
    ASSERT_TRUE(document);
    using Type = articulation::BracketEnclosure::Type;
    constexpr int REST = -1;
    struct Expected
    {
        EntryNumber entryNumber;
        std::vector<int> enclosedNoteIndices; ///< REST for a rest; empty for no enclosure.
        Type type;
        const char* description;
    };
    // Full-size notes in m5 and m9 may or may not count as enclosed, so they are not listed.
    // clang-format off
    const std::vector<Expected> expectedEntries = {
        { 125, {0},    Type::Parentheses, "m1: stock parentheses" },
        { 152, {0},    Type::Parentheses, "m1: stock parentheses, adjusted" },
        { 133, {0},    Type::Parentheses, "m2: lower chord note" },
        { 141, {1},    Type::Parentheses, "m2: upper chord note" },
        { 134, {1},    Type::Parentheses, "m2: upper chord note, down stem" },
        { 145, {0},    Type::Parentheses, "m2: lower chord note, down stem" },
        { 153, {0},    Type::Parentheses, "m3: stock parentheses on a grace note" },
        { 156, {0},    Type::Parentheses, "m3: manual parentheses on a grace note" },
        { 159, {1},    Type::Parentheses, "m4: stock parentheses on the upper note of a down-stem second" },
        { 160, {1},    Type::Parentheses, "m4: manual parentheses on the upper note of a down-stem second" },
        { 161, {0},    Type::Parentheses, "m4: manual parentheses on the lower note of an up-stem second" },
        { 162, {0, 1}, Type::Parentheses, "m4: both notes of an up-stem second" },
        { 163, {0},    Type::Parentheses, "m5: stock parentheses on a resized entry" },
        { 168, {0},    Type::Parentheses, "m5: manual parentheses on a resized entry" },
        { 170, {},     Type::Parentheses, "m6: centered on the stem" },
        { 174, {REST}, Type::Parentheses, "m7: floating rest" },
        { 175, {REST}, Type::Parentheses, "m7: positioned rest" },
        { 176, {REST}, Type::Brackets,    "m7: bracketed half rest" },
        { 179, {REST}, Type::Parentheses, "m8: half rest" },
        { 180, {REST}, Type::Parentheses, "m8: eighth rest" },
        { 182, {REST}, Type::Parentheses, "m8: stock parentheses on a rest" },
        { 183, {0},    Type::Parentheses, "m9: stock parentheses on a reduced notehead" },
        { 185, {0},    Type::Parentheses, "m9: manual parentheses on a reduced notehead" },
    };
    // clang-format on
    for (const auto& expected : expectedEntries) {
        const auto entryInfo = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, expected.entryNumber);
        ASSERT_TRUE(entryInfo) << expected.description;
        std::vector<int> enclosedNoteIndices;
        for (const auto& classification : classifyArticulations(entryInfo)) {
            if (const auto* enclosure = classification.as<articulation::BracketEnclosure>()) {
                EXPECT_EQ(enclosure->type, expected.type) << expected.description;
                enclosedNoteIndices.push_back(enclosure->note ? static_cast<int>(enclosure->note.getNoteIndex()) : REST);
            }
        }
        std::sort(enclosedNoteIndices.begin(), enclosedNoteIndices.end());
        EXPECT_EQ(enclosedNoteIndices, expected.enclosedNoteIndices) << expected.description;
    }
}
