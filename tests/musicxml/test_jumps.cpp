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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <sstream>

#include "core/musx_reader.h"
#include "formats/musicxml/musicxml.h"
#include "musx/musx.h"
#include "pugixml.hpp"
#include "test_utils.h"
#include "gtest/gtest.h"

namespace {

using namespace musx::dom;
using namespace denigma::formats::musicxml::detail;

constexpr MeasCmper kMeasureId = 1;
constexpr StaffCmper kFirstStaffId = 1;
constexpr StaffCmper kSecondStaffId = 2;
constexpr Cmper kLinkedPartId = 1;
constexpr Cmper kUnresolvedPartId = 2;

class MusicXmlJumps : public ::testing::Test
{
protected:
    void SetUp() override
    {
        setupTestDataPaths();
        pugi::xml_document optionsFixture;
        ASSERT_TRUE(optionsFixture.load_file(pathString(getInputPath() / "tuplet_completion.enigmaxml").c_str()));

        pugi::xml_document xml;
        ASSERT_TRUE(xml.load_string(R"xml(
<finale>
  <others>
    <partGlobals cmper="65534"/>
    <partGlobals cmper="65534" part="1" shared="false"/>
    <partGlobals cmper="65534" part="2" shared="false"/>
    <partDef cmper="0"/>
    <partDef cmper="1"/>
    <partDef cmper="2"/>
    <staffSpec cmper="1"><staffLines>5</staffLines></staffSpec>
    <staffSpec cmper="2"><staffLines>5</staffLines></staffSpec>
    <measSpec cmper="1"><txtRepeats/></measSpec>
    <textRepeatAssign cmper="1" inci="0"><action>stop</action><staffList>1</staffList></textRepeatAssign>
    <repeatStaffListScore cmper="1"><inst>-1</inst></repeatStaffListScore>
    <repeatStaffListParts cmper="1"><inst>-1</inst></repeatStaffListParts>
    <staffSystemSpec cmper="1"><startMeas>1</startMeas></staffSystemSpec>
    <staffSystemSpec cmper="2"/>
    <instUsed cmper="1" inci="0"><inst>1</inst></instUsed>
    <instUsed cmper="1" inci="1"><inst>2</inst></instUsed>
    <staffSystemSpec cmper="2" part="1" shared="false"><startMeas>1</startMeas></staffSystemSpec>
    <instUsed cmper="2" inci="0" part="1" shared="false"><inst>2</inst></instUsed>
    <instUsed cmper="2" inci="1" part="1" shared="false"><inst>1</inst></instUsed>
    <staffSystemSpec cmper="1" part="2" shared="false"/>
  </others>
</finale>)xml"));
        xml.child("finale").prepend_copy(optionsFixture.child("finale").child("options"));
        for (const auto& font : optionsFixture.child("finale").child("others").children("fontName")) {
            xml.child("finale").child("others").append_copy(font);
        }
        std::ostringstream stream;
        xml.save(stream);
        const auto text = stream.str();
        std::vector<char> buffer(text.begin(), text.end());
        document = musx::factory::DocumentFactory::create<denigma::MusxReader>(buffer);
        ASSERT_TRUE(document);
    }

    void expectJumpStaves(Cmper partId, bool firstStaffIncluded, bool secondStaffIncluded)
    {
        denigma::DenigmaContext denigmaContext(DENIGMA_NAME);
        MusicXmlMusxMapping mapping(denigmaContext, document, partId);
        ASSERT_FALSE(mapping.partLayoutIsCalculated);
        const auto measure = document->getOthers()->get<others::Measure>(partId, kMeasureId);
        ASSERT_TRUE(measure);
        mx::api::StaffData firstStaff;
        mx::api::StaffData secondStaff;
        processJumps(mapping, firstStaff, measure, kFirstStaffId, 0);
        processJumps(mapping, secondStaff, measure, kSecondStaffId, 1);
        EXPECT_EQ(firstStaff.directions.size(), firstStaffIncluded ? 1u : 0u);
        EXPECT_EQ(secondStaff.directions.size(), secondStaffIncluded ? 1u : 0u);
        for (const auto* staff : {&firstStaff, &secondStaff}) {
            for (const auto& direction : staff->directions) {
                EXPECT_TRUE(direction.isSoundDataSpecified);
                EXPECT_EQ(direction.soundData.fine, std::to_string(kMeasureId));
            }
        }
    }

    DocumentPtr document;
};

TEST_F(MusicXmlJumps, FiltersStaffListWithoutCalculatedPageLayout)
{
    expectJumpStaves(SCORE_PARTID, true, false);
}

TEST_F(MusicXmlJumps, UsesLinkedPartSystemStaves)
{
    expectJumpStaves(kLinkedPartId, false, true);
}

TEST_F(MusicXmlJumps, EmitsOnAllStavesWhenSystemCannotBeResolved)
{
    expectJumpStaves(kUnresolvedPartId, true, true);
}

} // namespace
