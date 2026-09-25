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

#include <optional>
#include <string>

#include "core/denigma.h"
#include "core/musx_reader.h"
#include "denigma/classify/barlines.h"
#include "formats/enigmaxml/enigmaxml.h"
#include "musx/musx.h"
#include "test_utils.h"

using namespace denigma::classify;
using namespace musx::dom;

namespace {

struct BarlineContext
{
    DocumentPtr document;
    MusxInstance<others::Staff> staff;
    MusxInstance<others::Measure> measure;
    MusxInstance<options::BarlineOptions> options;
};

static BarlineContext makeBarlineContext(std::string_view measureFields, std::string_view nextMeasureFields = {}, bool drawBarlines = true,
    bool drawFinalBarlineOnLastMeas = true, bool drawDoubleBarlineBeforeKeyChanges = false, bool hideStaffBarlines = false, int staffLines = 5,
    std::optional<Evpu> topBarlineOffset = std::nullopt, std::optional<Evpu> botBarlineOffset = std::nullopt)
{
    std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <options>
    <barlineOptions>
)xml";
    if (drawBarlines) {
        xml += "      <drawBarlines/>\n";
    }
    if (drawFinalBarlineOnLastMeas) {
        xml += "      <drawFinalBarlineOnLastMeas/>\n";
    }
    if (drawDoubleBarlineBeforeKeyChanges) {
        xml += "      <drawDoubleBarlineBeforeKeyChanges/>\n";
    }
    xml += R"xml(    </barlineOptions>
  </options>
  <others>
    <staffSpec cmper="1">
)xml";
    xml += "      <staffLines>" + std::to_string(staffLines) + "</staffLines>\n";
    if (topBarlineOffset) {
        xml += "      <topBarlineOffset>" + std::to_string(*topBarlineOffset) + "</topBarlineOffset>\n";
    }
    if (botBarlineOffset) {
        xml += "      <botBarlineOffset>" + std::to_string(*botBarlineOffset) + "</botBarlineOffset>\n";
    }
    if (hideStaffBarlines) {
        xml += "      <hideBarlines/>\n";
    }
    xml += R"xml(    </staffSpec>
    <measSpec cmper="1">
)xml";
    xml += measureFields;
    xml += R"xml(
    </measSpec>
)xml";
    if (!nextMeasureFields.empty()) {
        xml += R"xml(    <measSpec cmper="2">
)xml";
        xml += nextMeasureFields;
        xml += R"xml(
    </measSpec>
)xml";
    }
    xml += R"xml(  </others>
</finale>)xml";

    std::vector<char> buffer(xml.begin(), xml.end());
    auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
    return {
        document,
        document->getOthers()->get<others::Staff>(SCORE_PARTID, 1),
        document->getOthers()->get<others::Measure>(SCORE_PARTID, 1),
        document->getOptions()->get<options::BarlineOptions>(),
    };
}

static DocumentPtr loadFixture(const std::string& fileName)
{
    const auto inputPath = getInputPath() / fileName;
    denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
    denigmaContext.inputFilePath = inputPath;
    const auto inputData = denigma::formats::enigmaxml::detail::extractMusxInputData(inputPath, denigmaContext);
    return denigma::createMusxDocument<denigma::MusxReader>(inputData, denigmaContext);
}

} // namespace

TEST(BarlineClassification, ClassifiesSupportedMeasureTypes)
{
    const auto regular = makeBarlineContext("      <barline>normal</barline>");
    EXPECT_EQ(classifyBarline(regular.staff, regular.measure, false, regular.options).type, barline::Type::Regular);

    const auto doubleBar = makeBarlineContext("      <barline>double</barline>");
    EXPECT_EQ(classifyBarline(doubleBar.staff, doubleBar.measure, false, doubleBar.options).type, barline::Type::Double);

    const auto finalBar = makeBarlineContext("      <barline>final</barline>");
    EXPECT_EQ(classifyBarline(finalBar.staff, finalBar.measure, false, finalBar.options).type, barline::Type::Final);

    const auto heavy = makeBarlineContext("      <barline>solid</barline>");
    EXPECT_EQ(classifyBarline(heavy.staff, heavy.measure, false, heavy.options).type, barline::Type::Heavy);

    const auto dashed = makeBarlineContext("      <barline>dash</barline>");
    EXPECT_EQ(classifyBarline(dashed.staff, dashed.measure, false, dashed.options).type, barline::Type::Dashed);

    const auto tick = makeBarlineContext("      <barline>partial</barline>");
    EXPECT_EQ(classifyBarline(tick.staff, tick.measure, false, tick.options).type, barline::Type::Tick);
}

TEST(BarlineClassification, DefaultConstructsToUnsupported)
{
    const BarlineClassification classification;

    EXPECT_EQ(classification.type, barline::Type::Unsupported);
    EXPECT_FALSE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesHiddenBarlinesAsNoBarline)
{
    const auto optionsHidden = makeBarlineContext("      <barline>normal</barline>", {}, false);
    EXPECT_EQ(classifyBarline(optionsHidden.staff, optionsHidden.measure, false, optionsHidden.options).type, barline::Type::NoBarline);

    const auto staffHidden = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, true);
    EXPECT_EQ(classifyBarline(staffHidden.staff, staffHidden.measure, false, staffHidden.options).type, barline::Type::NoBarline);
}

TEST(BarlineClassification, UnsupportedTypesReturnUnsupported)
{
    const auto custom = makeBarlineContext("      <barline>custom</barline>");
    EXPECT_EQ(classifyBarline(custom.staff, custom.measure, false, custom.options).type, barline::Type::Unsupported);
}

TEST(BarlineClassification, ForcesRegularWhenFinalBarlineOptionIsDisabled)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, false);
    EXPECT_EQ(classifyBarline(context.staff, context.measure, true, context.options).type, barline::Type::Regular);
}

TEST(BarlineClassification, UsesDoubleBarlineBeforeKeyChanges)
{
    const auto context = makeBarlineContext(
        R"xml(      <barline>normal</barline>
      <keySig>
        <key>0</key>
      </keySig>)xml",
        R"xml(      <barline>normal</barline>
      <keySig>
        <key>1</key>
      </keySig>)xml",
        true, true, true);

    EXPECT_EQ(classifyBarline(context.staff, context.measure, false, context.options).type, barline::Type::Double);
}

TEST(BarlineClassification, ReportsShortFlag)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>");
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_FALSE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesShortBarlineAtCenter)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -48, 48);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, barline::Type::Regular);
    EXPECT_TRUE(classification.isShort);
}

TEST(BarlineClassification, ClassifiesShortBarlineAtMaximumDistanceFromCenter)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -12, 12);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, barline::Type::Regular);
    EXPECT_TRUE(classification.isShort);
}

TEST(BarlineClassification, RejectsShortBarlineOutsideDistanceFromCenterRange)
{
    const auto inverted = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -49, 49);
    EXPECT_FALSE(classifyBarline(inverted.staff, inverted.measure, false, inverted.options).isShort);

    const auto beyondMaximum = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -11, 11);
    EXPECT_FALSE(classifyBarline(beyondMaximum.staff, beyondMaximum.measure, false, beyondMaximum.options).isShort);
}

TEST(BarlineClassification, RejectsAsymmetricShortBarline)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 5, -24, 12);

    EXPECT_FALSE(classifyBarline(context.staff, context.measure, false, context.options).isShort);
}

TEST(BarlineClassification, ClassifiesOneLineShortBarlineAtMaximumDistanceFromCenter)
{
    const auto context = makeBarlineContext("      <barline>normal</barline>", {}, true, true, false, false, 1, 36, -36);
    const auto classification = classifyBarline(context.staff, context.measure, false, context.options);

    EXPECT_EQ(classification.type, barline::Type::Regular);
    EXPECT_TRUE(classification.isShort);
}

TEST(BarlineClassification, StackBarlineIsShortOnlyWhenShortOnEveryStaff)
{
    setupTestDataPaths();

    const auto mixed = loadFixture("barline_short_normal.musx");
    ASSERT_TRUE(mixed);
    const auto mixedMeasure = mixed->getOthers()->get<others::Measure>(SCORE_PARTID, 1);
    const auto mixedOptions = mixed->getOptions()->get<options::BarlineOptions>();
    const auto topStaff = mixed->getScrollViewStaves(SCORE_PARTID).at(0)->getStaffInstance(1, 0);
    EXPECT_TRUE(classifyBarline(topStaff, mixedMeasure, false, mixedOptions).isShort) << "top staff";
    const auto mixedStack = classifyBarline(nullptr, mixedMeasure, false, mixedOptions);
    EXPECT_EQ(mixedStack.type, barline::Type::Regular);
    EXPECT_FALSE(mixedStack.isShort) << "stack with a normal-length staff";

    const auto allShort = loadFixture("barline_types.musx");
    ASSERT_TRUE(allShort);
    constexpr MeasCmper shortMeasureId = 8;
    const auto allShortStack = classifyBarline(nullptr, allShort->getOthers()->get<others::Measure>(SCORE_PARTID, shortMeasureId), false,
        allShort->getOptions()->get<options::BarlineOptions>());
    EXPECT_EQ(allShortStack.type, barline::Type::Regular);
    EXPECT_TRUE(allShortStack.isShort) << "stack with every staff short";
}
