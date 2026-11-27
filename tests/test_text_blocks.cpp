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
#include <memory>
#include <string_view>
#include <vector>

#include "core/musx_reader.h"
#include "denigma/classify/gaps.h"
#include "denigma/classify/text_blocks.h"
#include "denigma/gap_report.h"
#include "musx/musx.h"
#include "nlohmann/json.hpp"
#include "gtest/gtest.h"

using namespace denigma;
using namespace musx::dom;

TEST(TextBlocks, FrameReportsStandardFrameInEvpu)
{
    // The frame is read from the text block alone, so a detached one suffices.
    others::TextBlock textBlock(DocumentWeakPtr{}, SCORE_PARTID, CommonClassBase::ShareMode::All, 1);
    textBlock.width = 288;
    textBlock.height = 144;
    textBlock.xAdd = 12;
    textBlock.yAdd = -6;
    textBlock.wordWrap = true;
    textBlock.justify = others::TextBlock::TextJustify::Full;
    textBlock.inset = Efix(EFIX_PER_EVPU * 3);
    textBlock.stdLineThickness = Efix(EFIX_PER_EVPU / 2);
    textBlock.showShape = true;
    textBlock.cornerRadius = Efix(EFIX_PER_EVPU * 4);
    textBlock.lineSpacingPercentage = 120;

    auto frame = classify::classifyTextBlockFrame(textBlock);
    EXPECT_EQ(frame.width, 288);
    EXPECT_EQ(frame.height, 144);
    EXPECT_EQ(frame.handleOffsetX, 12);
    EXPECT_EQ(frame.handleOffsetY, -6);
    EXPECT_TRUE(frame.wordWrap);
    EXPECT_EQ(frame.justify, others::TextBlock::TextJustify::Full);
    EXPECT_DOUBLE_EQ(frame.inset, 3.0);
    EXPECT_DOUBLE_EQ(frame.borderThickness, 0.5);
    EXPECT_TRUE(frame.showBorder);
    EXPECT_EQ(frame.lineSpacingPercent, 120);
    EXPECT_FALSE(frame.lineSpacing);
    // A corner radius means nothing until rounded corners are switched on.
    EXPECT_DOUBLE_EQ(frame.cornerRadius, 0.0);
    EXPECT_FALSE(frame.hasCustomShape);

    textBlock.roundCorners = true;
    textBlock.shapeId = 7;
    frame = classify::classifyTextBlockFrame(textBlock);
    EXPECT_DOUBLE_EQ(frame.cornerRadius, 4.0);
    EXPECT_TRUE(frame.hasCustomShape);
}

namespace {

// A score and one linked part sharing two page texts. The score has two pages and the part three,
// none of them laid out: there are no systems, so no page has a calculated layout. The first text is
// on every odd page and the part moves it; the second is on page 1 and hidden in the score only, and
// the part changes a right-page offset that nothing uses.
constexpr std::string_view pageTextXml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0"><charsetBank>Mac</charsetBank><charsetVal>0</charsetVal><pitch>0</pitch><family>0</family><name>Times</name></fontName>
    <pageSpec cmper="1"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>1</firstSystem></pageSpec>
    <pageSpec cmper="2"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>2</firstSystem></pageSpec>
    <pageSpec cmper="1" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>1</firstSystem></pageSpec>
    <pageSpec cmper="2" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>2</firstSystem></pageSpec>
    <pageSpec cmper="3" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>3</firstSystem></pageSpec>
    <partDef cmper="0"><partOrder>0</partOrder><copies>1</copies></partDef>
    <partDef cmper="1"><partOrder>1</partOrder><copies>1</copies></partDef>
    <textBlock cmper="1"><textID>1</textID><lineSpacingPercent>100</lineSpacingPercent><textTag>block</textTag></textBlock>
    <textBlock cmper="2"><textID>2</textID><lineSpacingPercent>100</lineSpacingPercent><textTag>block</textTag></textBlock>
    <pageTextAssign cmper="0" inci="0">
      <block>1</block>
      <ydisp>-48</ydisp>
      <startPage>1</startPage>
      <hposLp>right</hposLp>
      <oddEven>odd</oddEven>
    </pageTextAssign>
    <pageTextAssign cmper="0" inci="0" part="1" shared="true">
      <xdisp>24</xdisp>
      <ydisp>-48</ydisp>
      <hposLp>right</hposLp>
    </pageTextAssign>
    <pageTextAssign cmper="0" inci="1">
      <block>2</block>
      <startPage>1</startPage>
      <endPage>1</endPage>
      <postIt><offInPart/></postIt>
    </pageTextAssign>
    <pageTextAssign cmper="0" inci="1" part="1" shared="true">
      <postIt><offInPart/></postIt>
      <rightPgXdisp>36</rightPgXdisp>
    </pageTextAssign>
  </others>
  <texts>
    <blockText number="1">Page</blockText>
    <blockText number="2">Part only</blockText>
  </texts>
</finale>
)xml";

} // namespace

TEST(TextBlocks, PageTextPlacementsCoverEveryPartPage)
{
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(std::vector<char>(pageTextXml.begin(), pageTextXml.end()));
    ASSERT_TRUE(document);
    const auto texts = classify::classifyPageTexts(document, {SCORE_PARTID, 1});
    ASSERT_EQ(texts.size(), 2u);

    // The payload is the score's; each part's pages follow in turn, whether or not they were laid out.
    const auto& running = texts[0];
    ASSERT_TRUE(running.assignment);
    EXPECT_EQ(running.assignment->getRequestedPartId(), SCORE_PARTID);
    EXPECT_TRUE(running.assignment->isMultiAssignedThruLastPage());
    EXPECT_EQ(running.assignment->xDisp, 0);
    ASSERT_EQ(running.placements.size(), 3u);
    EXPECT_EQ(running.placements[0].partId, SCORE_PARTID);
    EXPECT_EQ(running.placements[0].page, 1);
    EXPECT_TRUE(running.placements[0].assignment->isPlacedLike(*running.assignment));
    EXPECT_EQ(running.placements[1].partId, 1);
    EXPECT_EQ(running.placements[1].page, 1);
    EXPECT_EQ(running.placements[2].page, 3) << "the part's even page 2 is skipped";
    // The part moved the text, so its placements carry its own assignment, placed differently.
    EXPECT_EQ(running.placements[2].assignment->xDisp, 24);
    EXPECT_FALSE(running.placements[2].assignment->isPlacedLike(*running.assignment));

    // Hidden in the score, the text is still the score's, and only the part places it.
    const auto& partOnly = texts[1];
    ASSERT_EQ(partOnly.text.runs.size(), 1u);
    EXPECT_EQ(partOnly.text.runs[0].text, "Part only");
    ASSERT_EQ(partOnly.placements.size(), 1u);
    EXPECT_EQ(partOnly.placements[0].partId, 1);
    EXPECT_EQ(partOnly.placements[0].page, 1);
    EXPECT_EQ(partOnly.assignment->getRequestedPartId(), SCORE_PARTID);
    // Right-hand pages are not positioned independently, so the part's right-page offset changes nothing.
    EXPECT_EQ(partOnly.placements[0].assignment->rightPgXDisp, 36);
    EXPECT_TRUE(partOnly.placements[0].assignment->isPlacedLike(*partOnly.assignment));

    // Classifying the part alone gives the same text and only the part's placements.
    const auto partTexts = classify::classifyPageTexts(document, {1});
    ASSERT_EQ(partTexts.size(), 2u);
    EXPECT_EQ(partTexts[0].placements.size(), 2u);
    EXPECT_EQ(partTexts[0].assignment->getRequestedPartId(), SCORE_PARTID) << "the payload is the score's";
}

namespace {

// The part has one leading blank page and the score none, so the document has one leading-blank
// slot: page assignment id 2 is the first page after the leading blank pages in every score.
constexpr std::string_view blankPageXml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <fontName cmper="0"><charsetBank>Mac</charsetBank><charsetVal>0</charsetVal><pitch>0</pitch><family>0</family><name>Times</name></fontName>
    <pageSpec cmper="1"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>1</firstSystem></pageSpec>
    <pageSpec cmper="2"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>2</firstSystem></pageSpec>
    <pageSpec cmper="1" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>-1</firstSystem></pageSpec>
    <pageSpec cmper="2" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>1</firstSystem></pageSpec>
    <pageSpec cmper="3" part="1" shared="false"><height>3168</height><width>2448</width><percent>100</percent><firstSystem>2</firstSystem></pageSpec>
    <partDef cmper="0"><partOrder>0</partOrder><copies>1</copies></partDef>
    <partDef cmper="1"><partOrder>1</partOrder><copies>1</copies></partDef>
    <textBlock cmper="1"><textID>1</textID><lineSpacingPercent>100</lineSpacingPercent><textTag>block</textTag></textBlock>
    <pageTextAssign cmper="0" inci="0">
      <block>1</block>
      <startPage>2</startPage>
      <endPage>2</endPage>
    </pageTextAssign>
  </others>
  <texts>
    <blockText number="1">First music page</blockText>
  </texts>
</finale>
)xml";

} // namespace

TEST(TextBlocks, PageAssignmentIdsShiftPastEachScoresLeadingBlankPages)
{
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(std::vector<char>(blankPageXml.begin(), blankPageXml.end()));
    ASSERT_TRUE(document);
    ASSERT_EQ(document->getMaxBlankPages(), 1);
    auto texts = classify::classifyPageTexts(document, {SCORE_PARTID, 1});
    ASSERT_EQ(texts.size(), 1u);
    ASSERT_EQ(texts[0].placements.size(), 2u);
    EXPECT_EQ(texts[0].placements[0].page, 1) << "the score has no leading blank page";
    EXPECT_EQ(texts[0].placements[1].page, 2) << "the part's first page is blank";

    // The report states the raw ids and, once, the slot count they are relative to.
    classify::GapCollector collector;
    collector.retainDocument(document);
    classify::PageTextGap gap{std::move(texts[0]), {{"score0.page1", 0}, {"score1.page2", 1}}};
    collector.addUnanchored(std::move(gap));
    const auto report = nlohmann::json::parse(serializeGapReport(collector, {{"denigma", "TEST", "abc123"}, {}}));
    EXPECT_EQ(report["leadingBlankPageSlots"], 1);
    const auto& pageText = report["gaps"][0]["pageText"];
    EXPECT_EQ(pageText["assignment"], nlohmann::json({{"start", 2}, {"end", 2}, {"pages", "all"}}));
    ASSERT_EQ(pageText["placements"].size(), 2u);
    EXPECT_EQ(pageText["placements"][1]["anchor"], "score1.page2");
}
