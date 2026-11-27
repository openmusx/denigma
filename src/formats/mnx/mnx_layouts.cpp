/*
 * Copyright (C) 2024, Robert Patterson
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
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "mnx.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

static std::optional<mnxdom::Part> findMnxPart(const MnxMusxMappingPtr& context, const std::string& partId)
{
    return context->mnxDocument->getEntityMap().tryGet<mnxdom::Part>(partId);
}

/// @brief Writes the name of a staff or group as a label ref or a label.
///
/// @p nameRef is the name the layout shows: `auto` for the scroll view, and `name` or `shortName` for a system
/// that shows full or abbreviated names.
///
/// When @p part is supplied, the name is that part's name: the scroll view refers to it with `auto`, and a
/// system refers to it only when the text Finale draws there is exactly the part's name or shortName. Part
/// names stop at their first line break, so a name that continues past one keeps its text as a label.
/// Without @p part, the name is plain text that no part carries.
template <typename LayoutObject>
static void setLayoutName(LayoutObject& mnxObject, mnxdom::LabelRefType nameRef, const std::optional<mnxdom::Part>& part, bool writeTarget,
    const std::string& fullName, const std::string& abrvName)
{
    const auto& name = nameRef == mnxdom::LabelRefType::ShortName ? abrvName : fullName;
    if (part) {
        const bool refersToPart = [&]() {
            switch (nameRef) {
            case mnxdom::LabelRefType::Auto: return part->name() || part->shortName();
            case mnxdom::LabelRefType::Name: return !name.empty() && part->name() == name;
            case mnxdom::LabelRefType::ShortName: return !name.empty() && part->shortName() == name;
            }
            return false;
        }();
        if (refersToPart) {
            auto labelRef = mnxObject.ensure_labelRef();
            labelRef.set_or_clear_ref(nameRef);
            if (writeTarget) {
                labelRef.set_target(part->id().value());
            }
            return;
        }
        if (nameRef == mnxdom::LabelRefType::Auto) {
            return; // the part has no name to show
        }
    }
    if (!name.empty()) {
        mnxObject.set_label(name);
    }
}

// Helper function to create a JSON representation of a single staff
static void buildMnxStaff(mnxdom::layout::Staff&& mnxStaff, const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& meas,
    const MusxInstance<others::StaffUsed>& staffSlot, mnxdom::LabelRefType nameRef)
{
    auto it = context->inst2Part.find(staffSlot->staffId);
    if (it == context->inst2Part.end()) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " was not assigned to any MNX part.");
    }
    auto staff = others::StaffComposite::createCurrent(context->document, staffSlot->getRequestedPartId(), staffSlot->staffId, meas->getCmper(), 0);
    if (!staff) {
        throw std::logic_error("Staff id " + std::to_string(staffSlot->staffId) + " does not have a Staff instance.");
    }
    auto mnxSource = mnxStaff.sources().append(it->second);
    const auto& instInfo = meas->getDocument()->getInstrumentForStaff(staffSlot->staffId);
    if (instInfo.staves.size() > 0) {
        if (const auto& staffIt = instInfo.staves.find(staffSlot->staffId); staffIt != instInfo.staves.end()) {
            mnxSource.set_staff(int(staffIt->second) + 1);
        }
    }
    if (staff->showNamesForPart(meas->getRequestedPartId())) {
        constexpr auto accidentalStyle = EnigmaString::AccidentalStyle::Unicode;
        if (staff->multiStaffInstId) {
            // A staff in a multi-staff instrument has its own name; the part's name belongs to the instrument's group.
            setLayoutName(mnxStaff, nameRef, std::nullopt, false, staff->getFullName(accidentalStyle), staff->getAbbreviatedName(accidentalStyle));
        } else {
            setLayoutName(mnxStaff, nameRef, findMnxPart(context, it->second), false, staff->getFullInstrumentName(accidentalStyle),
                staff->getAbbreviatedInstrumentName(accidentalStyle));
        }
    }
    if (!staff->hideStems) {
        switch (staff->stemDirection) {
        default: break;
        case StemDirection::AlwaysUp: mnxSource.set_stem(mnxdom::StemDirection::Up); break;
        case StemDirection::AlwaysDown: mnxSource.set_stem(mnxdom::StemDirection::Down); break;
        }
    }

    /** @todo (not sure if this will ever be done: will source from voiced parts settings if so)
    if (staffSlot->hasVoiceNumber()) {
        mnxSource.set_voice(staffSlot->getVoiceName());
    }
    */
}

static bool hasInstrumentStaffGroupMapping(const MnxMusxMappingPtr& context, const details::StaffGroupInfo& groupInfo)
{
    if (!groupInfo.startSlot) {
        return false;
    }
    const size_t slot = groupInfo.startSlot.value();
    if (slot >= groupInfo.systemStaves.size()) {
        return false;
    }

    const StaffCmper staffId = groupInfo.systemStaves[slot]->staffId;
    const auto& instInfo = context->document->getInstrumentForStaff(staffId);
    return instInfo.staffGroupId == groupInfo.group->getCmper2();
}

static void buildOrderedContent(mnxdom::layout::LayoutContent&& content, const MnxMusxMappingPtr& context,
    const std::vector<details::StaffGroupInfo>& groups, const MusxInstanceList<others::StaffUsed>& systemStaves,
    const MusxInstance<others::Measure> forMeas, mnxdom::LabelRefType nameRef, size_t fromIndex = 0,
    size_t toIndex = (std::numeric_limits<size_t>::max)(), size_t groupIndex = 0)
{
    json retval = json::array();
    size_t index = fromIndex;
    while (index < systemStaves.size()) {
        // Skip groups that have already ended
        while (groupIndex < groups.size() && *groups[groupIndex].endSlot < index) {
            groupIndex++;
        }
        if (groupIndex < groups.size() && index >= *groups[groupIndex].startSlot && index <= *groups[groupIndex].endSlot) {
            auto group = groups[groupIndex];
            auto mnxGroup = content.appendGroup();
            switch (group.group->drawBarlines) {
            case details::StaffGroup::DrawBarlineStyle::OnlyOnStaves:
                mnxGroup.set_or_clear_barlineStyle(mnxdom::StaffGroupBarlineStyle::Individual);
                break;
            case details::StaffGroup::DrawBarlineStyle::ThroughStaves:
                mnxGroup.set_or_clear_barlineStyle(hasInstrumentStaffGroupMapping(context, group) ? mnxdom::StaffGroupBarlineStyle::Instrument
                                                                                                  : mnxdom::StaffGroupBarlineStyle::Unified);
                break;
            case details::StaffGroup::DrawBarlineStyle::Mensurstriche:
                mnxGroup.set_or_clear_barlineStyle(mnxdom::StaffGroupBarlineStyle::Mensurstrich);
                break;
            }
            if (!group.group->hideName) {
                // An instrument's own group carries the name of the instrument's part.
                std::optional<mnxdom::Part> part;
                if (hasInstrumentStaffGroupMapping(context, group)) {
                    const StaffCmper staffId = group.systemStaves[group.startSlot.value()]->staffId;
                    if (const auto partIt = context->inst2Part.find(staffId); partIt != context->inst2Part.end()) {
                        part = findMnxPart(context, partIt->second);
                    }
                }
                setLayoutName(mnxGroup, nameRef, part, true, group.group->getFullInstrumentName(EnigmaString::AccidentalStyle::Unicode),
                    group.group->getAbbreviatedInstrumentName(EnigmaString::AccidentalStyle::Unicode));
            }
            if (group.startSlot != group.endSlot || group.group->bracket->showOnSingleStaff) {
                mnxGroup.set_or_clear_symbol(enumConvert<mnxdom::LayoutSymbol>(group.group->bracket->style));
            }
            // @todo add group properties to content
            buildOrderedContent(mnxGroup.content(), context, groups, systemStaves, forMeas, nameRef, index, *group.endSlot, groupIndex + 1);
            index = (std::max)(*group.endSlot + 1, index);
        } else {
            auto mnxStaff = content.appendStaff();
            buildMnxStaff(std::move(mnxStaff), context, forMeas, systemStaves[index++], nameRef);
        }
        if (index > toIndex) {
            break;
        }
    }
}

static void sortGroups(std::vector<details::StaffGroupInfo>& groups)
{
    std::sort(groups.begin(), groups.end(), [](const details::StaffGroupInfo& lhs, const details::StaffGroupInfo& rhs) {
        if (lhs.startSlot < rhs.startSlot) {
            return true; // Earlier start slot comes first
        }
        if (lhs.startSlot > rhs.startSlot) {
            return false;
        }
        if (lhs.endSlot > rhs.endSlot) {
            return true;    // Wider span comes first
        }
        if (lhs.endSlot < rhs.endSlot) {
            return false;
        }
        // Same span: prioritize by leftmost bracket
        if (lhs.group->bracket && rhs.group->bracket) {
            return lhs.group->bracket->horzAdjLeft < rhs.group->bracket->horzAdjLeft;
        }
        return false;
    });
}

/// @brief Builds one layout for @p linkedPart from the staff list identified by @p staffListCmper,
/// with staff groups as they stand at @p forMeas.
/// @param isScrollView True for the scroll-view layout, which leaves the choice of full or abbreviated names to the reader.
static void createLayout(const MnxMusxMappingPtr& context, const MusxInstance<others::PartDefinition>& linkedPart, Cmper staffListCmper,
    MeasCmper forMeas, bool isScrollView)
{
    auto& mnxDocument = context->mnxDocument;
    if (!mnxDocument->layouts()) {
        mnxDocument->ensure_layouts();
    }
    auto layout = mnxDocument->layouts().value().append();
    mnxDocument->assignId(layout, calcSystemLayoutId(linkedPart, staffListCmper));

    // Retrieve staff groups and staves in staff-list order.
    const auto systemStaves = context->document->getOthers()->getArray<others::StaffUsed>(linkedPart->getCmper(), staffListCmper);
    std::vector<details::StaffGroupInfo> groups = details::StaffGroupInfo::getGroupsAtMeasure(forMeas, linkedPart->getCmper(), systemStaves);
    sortGroups(groups);
    // Create a sequential content array.
    auto meas = context->document->getOthers()->get<others::Measure>(linkedPart->getCmper(), forMeas);
    if (!meas) {
        throw std::logic_error(
            "No Measure instance found for measure " + std::to_string(forMeas) + " in linked part " + std::to_string(linkedPart->getCmper()));
    }
    const auto nameRef =
        isScrollView ? mnxdom::LabelRefType::Auto : (meas->calcShouldShowFullNames() ? mnxdom::LabelRefType::Name : mnxdom::LabelRefType::ShortName);
    buildOrderedContent(layout.content(), context, groups, systemStaves, meas, nameRef);
}

void createLayouts(const MnxMusxMappingPtr& context)
{
    // Iterate over each linked part and generate layouts.
    for (const auto& linkedPart : context->musxParts) {
        // The scroll-view layout derives from calcScrollViewCmper and measure 1, so it needs no page layout.
        createLayout(context, linkedPart, linkedPart->calcScrollViewCmper(), 1, true);
        // Without a calculated layout, StaffSystem::startMeas is a zero placeholder, so per-system
        // layouts cannot be built at all: the measure lookup would find no measure 0. Emit only
        // the scroll-view layout. createScores omits this part's pages to match.
        if (!linkedPart->isLayoutCalculated()) {
            context->denigmaContext->logMessage(LogMsg() << "Part \"" << calcLinkedPartDisplayName(linkedPart)
                                                         << "\" has an uncalculated page layout; omitting its per-system layouts and pages.",
                MessageSeverity::Verbose);
            continue;
        }
        for (const auto& staffSystem : context->document->getOthers()->getArray<others::StaffSystem>(linkedPart->getCmper())) {
            // A system on no page is left over from a layout that ran past the last measure; no page refers to it.
            if (staffSystem->pageId) {
                createLayout(context, linkedPart, staffSystem->getCmper(), staffSystem->startMeas, false);
            }
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
