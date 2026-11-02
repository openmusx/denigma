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
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "core/musx_reader.h"
#include "formats/mnx/mnx_formatted_text.h"
#include "musx/musx.h"

namespace mnxdom = ::mnx;
using namespace denigma::formats::mnx::detail;
using namespace musx::dom;

namespace {

struct RawTextContext
{
    DocumentPtr document;
    MusxInstance<texts::BlockText> text;
    musx::util::EnigmaParsingContext parsingContext;
};

static RawTextContext makeRawTextContext(const std::string& enigmaText, bool nestedPartName = false)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Finale Maestro</name>
    </fontName>
    <fontName cmper="1">
      <charsetBank>Mac</charsetBank>
      <charsetVal>0</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Times New Roman</name>
    </fontName>
    <fontName cmper="2">
      <charsetBank>Mac</charsetBank>
      <charsetVal>4095</charsetVal>
      <pitch>0</pitch>
      <family>0</family>
      <name>Maestro</name>
    </fontName>
  </others>
  <texts>
    <blockText number="1">)xml";
    xml += enigmaText;
    xml += R"xml(</blockText>
  </texts>
</finale>)xml";

    if (nestedPartName) {
        xml.insert(xml.find("  </others>"), R"xml(
    <partDef cmper="0"><nameID>2</nameID></partDef>
    <textBlock cmper="2"><textID>2</textID><textTag>block</textTag></textBlock>
)xml");
        xml.insert(xml.find("  </texts>"), R"xml(
    <blockText number="2">Clarinet ^page(2)</blockText>
)xml");
    }
    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    auto text = document->getTexts()->get<texts::BlockText>(1);
    return {document, text, text->getRawTextCtx(text, SCORE_PARTID)};
}

} // namespace

TEST(MnxFormattedText, MergesFontChangesAndPreservesFormattingRanges)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(14)^nfx(3)Bold^nfx(0) plain^baseline(6)^superscript(3)^tracking(20) raised");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    EXPECT_EQ(mnxdom::json::parse(formatted.dump()), mnxdom::json::parse(R"json([
        {"id":"expr.text0","text":"Bold plain raised"}
    ])json"));
    ASSERT_EQ(gaps.gaps().size(), 3);
    const auto& first = std::get<denigma::classify::TextFormatting>(gaps.gaps()[0].payload);
    EXPECT_EQ(first.font.name, "Times New Roman");
    EXPECT_TRUE(first.font.bold);
    EXPECT_TRUE(first.font.italic);
    EXPECT_EQ(first.sourceText, "Bold");
    const auto& last = std::get<denigma::classify::TextFormatting>(gaps.gaps()[2].payload);
    EXPECT_EQ(last.baseline, 6);
    EXPECT_EQ(last.superscript, 3);
    EXPECT_EQ(last.tracking, 20);
    EXPECT_EQ(gaps.gaps()[2].textRanges[0].offset, 10);
    EXPECT_EQ(gaps.gaps()[2].textRanges[0].length, 7);
}

TEST(MnxFormattedText, SplitsMixedLegacySymbolsAndMergesGlyphFonts)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)p f^fontid(0)&#xE520;&#xE521;");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    EXPECT_EQ(mnxdom::json::parse(formatted.dump()), mnxdom::json::parse(R"json([
        {"id":"expr.text0","type":"smufl","glyphs":["dynamicPiano"]},
        {"id":"expr.text1","text":" "},
        {"id":"expr.text2","type":"smufl","glyphs":["dynamicForte","dynamicPiano","dynamicMezzo"]}
    ])json"));
    ASSERT_EQ(gaps.gaps().size(), 4);
    const auto& legacy = std::get<denigma::classify::TextFormatting>(gaps.gaps()[2].payload);
    EXPECT_EQ(legacy.sourceText, "f");
    EXPECT_EQ(legacy.font.name, "Maestro");
    EXPECT_EQ(gaps.gaps()[3].textRanges[0].offset, 1);
    EXPECT_EQ(gaps.gaps()[3].textRanges[0].length, 2);
}

TEST(MnxFormattedText, CountsUnicodeScalarsAndPreservesWhitespace)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)&#x1D12A;é ^nfx(1)B\n ");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    const auto json = mnxdom::json::parse(formatted.dump());
    ASSERT_EQ(json.size(), 1);
    EXPECT_EQ(json[0]["text"], "𝄪é B\n ");
    ASSERT_EQ(gaps.gaps().size(), 2);
    EXPECT_EQ(gaps.gaps()[1].textRanges[0].offset, 3);
    EXPECT_EQ(gaps.gaps()[1].textRanges[0].length, 3);
}

TEST(MnxFormattedText, KeepsRepeatedInsertRangesWithinMergedText)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)p.^page(3)^page(3)^^");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    EXPECT_EQ(mnxdom::json::parse(formatted.dump())[0]["text"], "p.##^");
    ASSERT_EQ(gaps.gaps().size(), 3);
    EXPECT_EQ(gaps.gaps()[0].textRanges[0].length, 5);
    for (size_t i = 1; i < gaps.gaps().size(); ++i) {
        const auto& gap = gaps.gaps()[i];
        const auto& insert = std::get<denigma::classify::TextInsert>(gap.payload);
        EXPECT_EQ(insert.insert.command, "page");
        EXPECT_EQ(insert.insert.parameters, std::vector<std::string>{"3"});
        EXPECT_EQ(insert.sourceText, "#");
        EXPECT_EQ(gap.textRanges[0].offset, i + 1);
        EXPECT_EQ(gap.textRanges[0].length, 1);
    }
}

TEST(MnxFormattedText, MapsOneMixedInsertToSeveralOutputElements)
{
    auto ctx = makeRawTextContext("^fontid(2)^size(24)^nfx(0)^value()");
    ctx.parsingContext = musx::util::EnigmaParsingContext(
        ctx.text, SCORE_PARTID, std::nullopt, [](const std::vector<std::string>& components) -> std::optional<std::string> {
            if (components[0] == "value") {
                return "p f";
            }
            return std::nullopt;
        });
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    ASSERT_EQ(mnxdom::json::parse(formatted.dump()).size(), 3);
    ASSERT_EQ(gaps.gaps().size(), 4);
    const auto& insert = gaps.gaps().back();
    EXPECT_EQ(std::get<denigma::classify::TextInsert>(insert.payload).sourceText, "p f");
    ASSERT_EQ(insert.textRanges.size(), 3);
    EXPECT_EQ(insert.textRanges[0].id, "expr.text0");
    EXPECT_EQ(insert.textRanges[1].id, "expr.text1");
    EXPECT_EQ(insert.textRanges[2].id, "expr.text2");
}

TEST(MnxFormattedText, PreservesAccidentalInsertIdentity)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)B^flat()");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    const auto json = mnxdom::json::parse(formatted.dump());
    ASSERT_EQ(json.size(), 2);
    EXPECT_EQ(json[1]["glyphs"], mnxdom::json::array({"accidentalFlat"}));
    ASSERT_EQ(gaps.gaps().size(), 3);
    const auto& insert = std::get<denigma::classify::TextInsert>(gaps.gaps().back().payload);
    EXPECT_EQ(insert.insert.kind, denigma::classify::text::Insert::Kind::Accidental);
    EXPECT_EQ(insert.sourceText, "♭");
}

TEST(MnxFormattedText, DropsHiddenAndEmptySubstitutionsWithoutBreakingMerging)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)A^composer()^nfx(128)hidden^nfx(0)B");
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    EXPECT_EQ(mnxdom::json::parse(formatted.dump())[0]["text"], "AB");
    ASSERT_EQ(gaps.gaps().size(), 1);
    EXPECT_EQ(std::get<denigma::classify::TextFormatting>(gaps.gaps()[0].payload).sourceText, "AB");
}

TEST(MnxFormattedText, ReplacesExistingContentAndIgnoresGapCollectionForOutput)
{
    auto first = makeRawTextContext("^fontid(1)^size(12)^nfx(0)First");
    auto second = makeRawTextContext("^fontid(1)^size(12)^nfx(0)Second");
    auto formatted = makeFormattedText(first.parsingContext, "expr");
    denigma::classify::GapCollector gaps;
    setFormattedText(formatted, denigma::classify::classifyFormattedText(second.parsingContext), "expr", &gaps);
    EXPECT_EQ(formatted.dump(), makeFormattedText(second.parsingContext, "expr").dump());
}

TEST(MnxFormattedText, OmitsEntirelyHiddenContent)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(128)hidden");
    denigma::classify::GapCollector gaps;
    EXPECT_TRUE(makeFormattedText(ctx.parsingContext, "expr", &gaps).empty());
    EXPECT_TRUE(gaps.gaps().empty());
}

TEST(MnxFormattedText, ReportsOnlyParserProvidedProvenanceForNestedInserts)
{
    auto ctx = makeRawTextContext("^fontid(1)^size(12)^nfx(0)^partname()", true);
    denigma::classify::GapCollector gaps;
    const auto formatted = makeFormattedText(ctx.parsingContext, "expr", &gaps);
    EXPECT_EQ(mnxdom::json::parse(formatted.dump())[0]["text"], "Clarinet #");
    ASSERT_EQ(gaps.gaps().size(), 3);
    const auto& outer = std::get<denigma::classify::TextInsert>(gaps.gaps()[1].payload);
    const auto& inner = std::get<denigma::classify::TextInsert>(gaps.gaps()[2].payload);
    EXPECT_EQ(outer.insert.command, "partname");
    EXPECT_EQ(outer.sourceText, "Clarinet ");
    EXPECT_EQ(inner.insert.command, "page");
    EXPECT_EQ(inner.sourceText, "#");
    EXPECT_EQ(gaps.gaps()[2].textRanges[0].offset, 9);
}
