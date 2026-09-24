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
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "denigma/classify/tuplets.h"
#include "core/denigma.h"

#include <stdexcept>

namespace denigma {
namespace classify {

TupletClassification classifyTuplet(const musx::dom::EntryFrame::TupletInfo& tupletInfo, const musx::dom::EntryInfoPtr& firstEntryInfo)
{
    using PositioningStyle = musx::dom::details::TupletDef::PositioningStyle;
    using AutoBracketStyle = musx::dom::details::TupletDef::AutoBracketStyle;
    using BracketStyle = musx::dom::details::TupletDef::BracketStyle;
    using VerticalPlacement = musx::dom::VerticalPlacement;

    ASSERT_IF (!firstEntryInfo) {
        throw std::invalid_argument("classifyTuplet requires a first entry.");
    }

    TupletClassification result;
    const auto& tuplet = tupletInfo.tuplet;
    const bool firstEntryHasUpStem = firstEntryInfo.calcUpStem();
    switch (tuplet->posStyle) {
    case PositioningStyle::Manual:
        // Finale's whole-number vertical offset is tupOffY; nonnegative positions are over the notes.
        result.placement = tuplet->tupOffY >= 0 ? VerticalPlacement::Above : VerticalPlacement::Below;
        break;
    case PositioningStyle::BeamSide: result.placement = firstEntryHasUpStem ? VerticalPlacement::Above : VerticalPlacement::Below; break;
    case PositioningStyle::NoteSide: result.placement = firstEntryHasUpStem ? VerticalPlacement::Below : VerticalPlacement::Above; break;
    case PositioningStyle::Above: result.placement = VerticalPlacement::Above; break;
    case PositioningStyle::Below: result.placement = VerticalPlacement::Below; break;
    }

    result.showNumber = !tuplet->hidden && tuplet->numStyle != musx::dom::details::TupletDef::NumberStyle::Nothing;
    result.showBracket = !tuplet->hidden && tuplet->brackStyle != BracketStyle::Nothing;
    if (!result.showBracket) {
        return result;
    }

    const auto calcIsBeamedTogether = [&]() {
        // A tuplet is beamed only when one beam group spans both ends.
        bool result = tupletInfo.startIndex < tupletInfo.endIndex;
        if (result) {
            auto lastBeamMember = firstEntryInfo;
            while (lastBeamMember && lastBeamMember.getIndexInFrame() < tupletInfo.endIndex) {
                lastBeamMember = lastBeamMember.getNextInBeamGroup();
            }
            result = lastBeamMember && lastBeamMember.getIndexInFrame() >= tupletInfo.endIndex;
        }
        return result;
    };

    switch (tuplet->autoBracketStyle) {
    case AutoBracketStyle::Always: break;
    case AutoBracketStyle::UnbeamedOnly: result.showBracket = !calcIsBeamedTogether(); break;
    case AutoBracketStyle::NeverBeamSide:
        if (calcIsBeamedTogether()) {
            const auto beamSide = firstEntryHasUpStem ? VerticalPlacement::Above : VerticalPlacement::Below;
            result.showBracket = result.placement != beamSide;
        }
        break;
    }

    return result;
}

} // namespace classify
} // namespace denigma
