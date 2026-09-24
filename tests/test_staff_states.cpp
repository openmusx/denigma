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
#include "denigma/classify/staff_states.h"

using namespace denigma::classify;
using namespace musx::dom;
using musx::util::Fraction;

namespace {

/// Builds a four-measure 4/4 document. Staff 1 has a one-line staff style from the middle of
/// measure 2 to its third beat, and again from the middle of measure 3 to the end. Staff 2 has no
/// staff styles.
DocumentPtr makeStaffStyleDocument()
{
    const std::string xml = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<finale>
  <others>
    <measSpec cmper="1"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <measSpec cmper="2"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <measSpec cmper="3"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <measSpec cmper="4"><beats>4</beats><divbeat>1024</divbeat></measSpec>
    <staffSpec cmper="1"><staffLines>5</staffLines><lineSpace>24</lineSpace><hasStyles/></staffSpec>
    <staffSpec cmper="2"><staffLines>5</staffLines><lineSpace>24</lineSpace></staffSpec>
    <staffStyle cmper="1">
      <staffLines>1</staffLines>
      <lineSpace>24</lineSpace>
      <styleName>One line</styleName>
      <mask><staffType/></mask>
    </staffStyle>
    <staffStyleAssign cmper="1" inci="0">
      <style>1</style>
      <startMeas>2</startMeas>
      <startEdu>2048</startEdu>
      <endMeas>2</endMeas>
      <endEdu>3071</endEdu>
    </staffStyleAssign>
    <staffStyleAssign cmper="1" inci="1">
      <style>1</style>
      <startMeas>3</startMeas>
      <startEdu>2048</startEdu>
      <endMeas>4</endMeas>
      <endEdu>4095</endEdu>
    </staffStyleAssign>
  </others>
</finale>
)xml";
    std::vector<char> buffer(xml.begin(), xml.end());
    return musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
}

std::vector<StaffStateChange> collectChanges(const DocumentPtr& document, StaffCmper staffId, const StaffState& baseline = {})
{
    std::vector<StaffStateChange> result;
    const bool completed = iterateStaffStateChanges(document, SCORE_PARTID, staffId, baseline, [&](const StaffStateChange& change) {
        result.push_back(change);
        return true;
    });
    EXPECT_TRUE(completed);
    return result;
}

} // namespace

TEST(StaffStateClassification, DefaultStaffReportsNothing)
{
    const auto document = makeStaffStyleDocument();
    EXPECT_TRUE(collectChanges(document, 2).empty());
}

TEST(StaffStateClassification, ReportsStaffStyleStartsAndEnds)
{
    const auto document = makeStaffStyleDocument();
    const auto changes = collectChanges(document, 1);
    ASSERT_EQ(changes.size(), 3u);

    EXPECT_EQ(changes[0].point, MusicPoint(2, Fraction(1, 2)));
    EXPECT_EQ(changes[0].previous.numberOfLines, 5);
    EXPECT_EQ(changes[0].current.numberOfLines, 1);
    EXPECT_TRUE(changes[0].staff);

    EXPECT_EQ(changes[1].point, MusicPoint(2, Fraction(3, 4)));
    EXPECT_EQ(changes[1].previous.numberOfLines, 1);
    EXPECT_EQ(changes[1].current.numberOfLines, 5);

    // The second range runs to the end of the document, so nothing restores the staff after it.
    EXPECT_EQ(changes[2].point, MusicPoint(3, Fraction(1, 2)));
    EXPECT_EQ(changes[2].current.numberOfLines, 1);

    for (const auto& change : changes) {
        EXPECT_EQ(change.current.lineSpace, change.previous.lineSpace);
        EXPECT_EQ(change.current.staffScaling, Fraction(1));
        EXPECT_EQ(change.current.transposition, change.previous.transposition);
    }
}

TEST(StaffStateClassification, BaselineReplacesDefaults)
{
    const auto document = makeStaffStyleDocument();
    StaffState baseline;
    baseline.numberOfLines = 1;

    const auto defaultStaffChanges = collectChanges(document, 2, baseline);
    ASSERT_EQ(defaultStaffChanges.size(), 1u);
    EXPECT_EQ(defaultStaffChanges[0].point, MusicPoint{});
    EXPECT_EQ(defaultStaffChanges[0].previous, baseline);
    EXPECT_EQ(defaultStaffChanges[0].current.numberOfLines, 5);

    const auto styledStaffChanges = collectChanges(document, 1, calcStaffState(document, SCORE_PARTID, 1, MusicPoint{}));
    ASSERT_FALSE(styledStaffChanges.empty());
    EXPECT_NE(styledStaffChanges[0].point, MusicPoint{});
}

TEST(StaffStateClassification, CalcStaffStateMatchesWalk)
{
    const auto document = makeStaffStyleDocument();
    for (const auto& change : collectChanges(document, 1)) {
        EXPECT_EQ(calcStaffState(document, SCORE_PARTID, 1, change.point), change.current);
    }
    EXPECT_EQ(calcStaffState(document, SCORE_PARTID, 1, MusicPoint(2, Fraction(1, 4))).numberOfLines, 5);
}

TEST(StaffStateClassification, CallbackCanStopWalk)
{
    const auto document = makeStaffStyleDocument();
    int calls = 0;
    const bool completed = iterateStaffStateChanges(document, SCORE_PARTID, 1, {}, [&](const StaffStateChange&) {
        ++calls;
        return false;
    });
    EXPECT_FALSE(completed);
    EXPECT_EQ(calls, 1);
}
