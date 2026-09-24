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
#include <filesystem>
#include <memory>
#include <vector>

#include "core/element_ids.h"
#include "core/musx_reader.h"
#include "musx/musx.h"
#include "test_utils.h"
#include "gtest/gtest.h"

using namespace denigma;
using namespace musx::dom;

TEST(ElementIdsTest, CalcEventId)
{
    std::vector<char> xml;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), xml);
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);
    const auto firstEntry = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 1);
    const auto secondEntry = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 2);
    ASSERT_TRUE(firstEntry);
    ASSERT_TRUE(secondEntry);
    EXPECT_EQ(core::calcEventId(firstEntry), "ev1");
    EXPECT_EQ(core::calcEventId(secondEntry), "ev2");
}

TEST(ElementIdsTest, CalcGlobalMeasureId)
{
    EXPECT_EQ(core::calcGlobalMeasureId(1), "m1");
    EXPECT_EQ(core::calcGlobalMeasureId(42), "m42");
}

TEST(ElementIdsTest, CalcPartMeasureId)
{
    EXPECT_EQ(core::calcPartMeasureId("P1", 1), "P1.m1");
    EXPECT_EQ(core::calcPartMeasureId("P2", 42), "P2.m42");
}

TEST(ElementIdsTest, CalcTempoDefId)
{
    EXPECT_EQ(core::calcTempoDefId(3, 0), "m3.tempoDef.inci0");
    EXPECT_EQ(core::calcTempoDefId(12, 4), "m12.tempoDef.inci4");
}

namespace {

// The assignment records provenance without consulting its document, so a detached one suffices.
template <typename T>
MusxInstance<T> makeLyricAssign(EntryNumber entry, Cmper lyricNumber, Inci inci)
{
    auto assign = std::make_shared<T>(DocumentWeakPtr{}, SCORE_PARTID, CommonClassBase::ShareMode::All, entry, inci);
    assign->lyricNumber = lyricNumber;
    return assign;
}

} // namespace

TEST(ElementIdsTest, CalcLyricAssignId)
{
    std::vector<char> xml;
    readFile(getInputPath() / "reference" / utils::utf8ToPath("notAscii-其れ.enigmaxml"), xml);
    const auto document = musx::factory::DocumentFactory::create<denigma::MusxReader>(xml);
    ASSERT_TRUE(document);
    const auto firstEntry = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 1);
    const auto secondEntry = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 2);
    const auto thirdEntry = EntryInfoPtr::fromEntryNumber(document, SCORE_PARTID, 3);
    ASSERT_TRUE(firstEntry);
    ASSERT_TRUE(secondEntry);
    ASSERT_TRUE(thirdEntry);
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignVerse>(1, 1, 0), firstEntry), "ev1.verse1.inci0");
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignChorus>(2, 2, 1), secondEntry), "ev2.chorus2.inci1");
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignSection>(3, 3, 2), thirdEntry), "ev3.section3.inci2");
}
