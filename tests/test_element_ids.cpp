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

#include "core/element_ids.h"
#include "musx/musx.h"
#include "gtest/gtest.h"

using namespace denigma;
using namespace musx::dom;

TEST(ElementIdsTest, CalcEventId)
{
    EXPECT_EQ(core::calcEventId(125), "ev125");
    EXPECT_EQ(core::calcEventId(0), "ev0");
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
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignVerse>(20, 1, 0)), "ev20.verse1.inci0");
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignChorus>(137, 2, 1)), "ev137.chorus2.inci1");
    EXPECT_EQ(core::calcLyricAssignId(makeLyricAssign<details::LyricAssignSection>(5, 3, 2)), "ev5.section3.inci2");
}
