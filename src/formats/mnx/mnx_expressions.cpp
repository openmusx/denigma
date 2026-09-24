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

#include "mnx_expressions.h"

#include <map>
#include <string>
#include <vector>

#include "core/element_ids.h"
#include "denigma/classify/expressions.h"
#include "denigma/classify/gaps.h"
#include "mnx.h"
#include "mnx_gaps.h"
#include "utils/stringutils.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {

/// @brief Where the target would draw each member of a staff-list group, in target ids.
///
/// A floating member names the system edge and the global measure. A concrete member names the
/// part measure and, when the part has several staves, the staff. A staff that maps to no part
/// (a staff outside every instrument) is left out.
std::vector<classify::GapPlacement> placementsForGroup(
    const MnxMusxMappingPtr& context, const classify::ExpressionAssignmentGroup& group, const std::string& globalMeasureId)
{
    using Kind = classify::GapPlacement::Kind;
    std::vector<classify::GapPlacement> result;
    for (const auto& member : group.members) {
        switch (classify::classifyExpressionScope(member)) {
        case classify::ExpressionScope::TopStaff: result.push_back({Kind::SystemTop, {globalMeasureId, std::nullopt, std::nullopt}}); break;
        case classify::ExpressionScope::BottomStaff: result.push_back({Kind::SystemBottom, {globalMeasureId, std::nullopt, std::nullopt}}); break;
        case classify::ExpressionScope::Staff:
            if (auto anchor = partMeasureAnchor(context, member->staffAssign, member->getCmper(), std::nullopt)) {
                result.push_back({Kind::Staff, std::move(*anchor)});
            }
            break;
        case classify::ExpressionScope::Unassigned: break;
        }
    }
    return result;
}

/// @brief True for the expression types MNX exports as a global tempo.
bool isTempoFamily(classify::ExpressionType type)
{
    return type == classify::ExpressionType::TempoMark || type == classify::ExpressionType::MetronomeMark;
}

/// @brief True for the expression types that belong to the global measure rather than to a staff.
/// The choice is by type alone, whatever staff or staff list Finale assigned; see design-decisions.md.
bool isGlobalExpressionType(classify::ExpressionType type)
{
    return isTempoFamily(type) || type == classify::ExpressionType::TempoAlteration || type == classify::ExpressionType::RehearsalMark;
}

struct ExpressionAttachmentContext
{
    EntryInfoPtr entryInfo;
    const EntryTarget* entryTarget{nullptr};
};

ExpressionAttachmentContext calcAttachmentContext(const MnxMusxMappingPtr& context, const MusxInstance<others::MeasureExprAssign>& asgn)
{
    ExpressionAttachmentContext result;
    result.entryInfo = asgn->calcAssociatedEntry();
    if (!result.entryInfo) {
        return result;
    }
    const auto entryNumber = result.entryInfo->getEntry()->getEntryNumber();
    const auto entryIt = context->entryTargetByNumber.find(entryNumber);
    if (entryIt != context->entryTargetByNumber.end()) {
        result.entryTarget = &entryIt->second;
    }
    return result;
}

struct MnxDynamicProjection
{
    classify::dynamics::Dynamic dynamic{};
    classify::dynamics::Composition composition;
    std::string prefixText;
    std::string suffixText;
    std::vector<std::string> glyphs;
    classify::dynamics::Change change{classify::dynamics::Change::Absolute};

    [[nodiscard]] bool containsText() const noexcept { return !prefixText.empty() || !glyphs.empty() || !suffixText.empty(); }
};

std::optional<MnxDynamicProjection> projectPrimaryDynamicForMnx(const classify::ExpressionClassification& classification)
{
    auto appendText = [](std::string& dest, const classify::expression::RunClassification& run) { dest += run.chunk.text; };
    auto mergeQualifier = [](classify::dynamics::Change& current, classify::dynamics::Change next) {
        if (next == classify::dynamics::Change::Absolute) {
            return true;
        }
        if (current == classify::dynamics::Change::Absolute) {
            current = next;
            return true;
        }
        return current == next;
    };

    MnxDynamicProjection result;
    bool sawDynamic = false;
    bool afterDynamic = false;
    for (const auto& run : classification.runs) {
        if (const auto* dynamicMark = run.as<classify::dynamics::Mark>()) {
            if (sawDynamic) {
                return std::nullopt;
            }
            sawDynamic = true;
            result.dynamic = dynamicMark->dynamic;
            result.composition = dynamicMark->composition;
            result.glyphs = dynamicMark->glyphs;
            afterDynamic = true;
            continue;
        }

        if (const auto* genericText = run.as<classify::expression::GenericText>()) {
            (void)genericText;
            if (afterDynamic) {
                appendText(result.suffixText, run);
            } else {
                appendText(result.prefixText, run);
            }
            continue;
        }

        if (const auto* qualifier = run.as<classify::expression::DynamicQualifier>()) {
            if (!mergeQualifier(result.change, qualifier->change)) {
                return std::nullopt;
            }
            if (afterDynamic) {
                appendText(result.suffixText, run);
            } else {
                appendText(result.prefixText, run);
            }
            continue;
        }

        return std::nullopt;
    }

    if (!sawDynamic || result.dynamic == classify::dynamics::Dynamic{}) {
        return std::nullopt;
    }
    result.prefixText = utils::trimAscii(result.prefixText);
    result.suffixText = utils::trimAscii(result.suffixText);
    if (result.dynamic == classify::dynamics::Dynamic::Other && result.glyphs.empty() && result.prefixText.empty() && result.suffixText.empty()) {
        const auto dynamicIt = std::find_if(classification.runs.begin(), classification.runs.end(),
            [](const classify::expression::RunClassification& run) { return std::holds_alternative<classify::dynamics::Mark>(run.value); });
        if (dynamicIt != classification.runs.end()) {
            result.prefixText = utils::trimAscii(dynamicIt->chunk.text);
        }
    }

    return result;
}

/// @brief MNX projection of a classified dynamic's structure.
struct MnxDynamicShape
{
    /// @brief The sounding level. Absent when the source dynamic has no MNX equivalent.
    std::optional<mnxdom::DynamicValue> value;
    /// @brief The level that remains after the attack, e.g. "p" for "fp".
    std::optional<mnxdom::DynamicValue> residualValue;
    /// @brief The leading syllable. MNX defaults this to "s", so it must be set explicitly
    /// for every accent that is not sforzando.
    mnxdom::DynamicPrefix accentPrefix{mnxdom::DynamicPrefix::None};
    /// @brief The trailing syllable. MNX defaults this to "z", so it must be set explicitly
    /// for every accent that is not forzato.
    mnxdom::DynamicSuffix accentSuffix{mnxdom::DynamicSuffix::None};
    /// @brief Whether MNX represents the dynamic as an accent rather than an immediate value.
    bool isAccent{};
};

/// @brief Maps a level to its MNX value, or std::nullopt for a level MNX cannot name.
std::optional<mnxdom::DynamicValue> calcDynamicValue(classify::dynamics::Level level)
{
    using Level = classify::dynamics::Level;
    if (level == Level::None || level == Level::Other) {
        return std::nullopt;
    }
    return enumConvert<mnxdom::DynamicValue>(level);
}

/// @brief Projects a classified dynamic's structure onto MNX. Driven by the composition rather than
/// the dynamics::Dynamic enumerator, so a marking outside that vocabulary, such as "sffffffz",
/// exports in full as long as MNX has a value for each of its levels.
MnxDynamicShape calcDynamicShape(const classify::dynamics::Composition& composition)
{
    MnxDynamicShape result;
    result.value = calcDynamicValue(composition.level);
    result.residualValue = calcDynamicValue(composition.subsequent);
    result.accentPrefix = enumConvert<mnxdom::DynamicPrefix>(composition.reinforcement);
    result.accentSuffix = composition.forzato ? mnxdom::DynamicSuffix::z : mnxdom::DynamicSuffix::None;
    const bool hasAffix = composition.reinforcement != classify::dynamics::Reinforcement::None || composition.forzato
                          || composition.subsequent != classify::dynamics::Level::None;
    result.isAccent = hasAffix && result.value.has_value();
    return result;
}

void appendDynamic(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::MeasureExprAssign>& asgn, const classify::ExpressionClassification& classification, VerticalPlacement placement)
{
    if (asgn->layer > 0 && context->current.cuePlan.isCueLayer(asgn->layer - 1)) {
        return;
    }
    // A dynamic MNX cannot spell is a gap like any other expression it cannot represent.
    const auto reportGap = [&]() {
        if (auto* const gapCollector = gapCollectorFor(context)) {
            gapCollector->add(measureAnchor(mnxMeasure.id_or(""), mnxStaffNumber, asgn->eduPosition), classification);
        }
    };

    auto dynamicClass = projectPrimaryDynamicForMnx(classification);
    if (!dynamicClass) {
        reportGap();
        return;
    }

    const auto shape = calcDynamicShape(dynamicClass->composition);
    if (!shape.value && (dynamicClass->change == classify::dynamics::Change::Absolute || !dynamicClass->containsText())) {
        reportGap();
        return;
    }

    // MNX now spells out every dynamic the classifier recognizes, so the source glyphs no longer have
    // to stand in for a value MNX cannot express. They are still passed through, because they describe
    // the appearance the document actually uses and override the default rendering of the value.
    std::vector<std::string> glyphs = std::move(dynamicClass->glyphs);

    auto mnxDynamic = [&]() -> mnxdom::part::DynamicGroupBase {
        using DynRelType = classify::dynamics::Change;
        if (dynamicClass->change != DynRelType::Absolute) {
            auto relValue =
                dynamicClass->change == DynRelType::RelativeIncrease ? mnxdom::DynamicRelativeValue::Louder : mnxdom::DynamicRelativeValue::Softer;
            auto dyn = mnxMeasure.ensure_dynamics().appendRelative(relValue, mnxFractionFromEdu(asgn->eduPosition));
            if (shape.value) {
                dyn.set_value(shape.value.value());
            }
            return dyn;
        } else if (shape.isAccent) {
            auto dyn = mnxMeasure.ensure_dynamics().appendAccent(shape.value.value(), mnxFractionFromEdu(asgn->eduPosition));
            // Both affixes have non-empty defaults in MNX, so an accent that omits them reads as "sfz".
            dyn.set_accentPrefix(shape.accentPrefix);
            dyn.set_accentSuffix(shape.accentSuffix);
            if (shape.residualValue) {
                dyn.set_residualValue(shape.residualValue.value());
            }
            return dyn;
        } else {
            return mnxMeasure.ensure_dynamics().appendImmediate(shape.value.value(), mnxFractionFromEdu(asgn->eduPosition));
        }
    }();
    if (!dynamicClass->prefixText.empty()) {
        mnxDynamic.set_prefix(dynamicClass->prefixText);
    }
    if (!dynamicClass->suffixText.empty()) {
        mnxDynamic.set_suffix(dynamicClass->suffixText);
    }
    if (!glyphs.empty()) {
        mnxDynamic.ensure_glyphs().assign(glyphs);
    }
    const auto entryInfo = asgn->calcAssociatedEntry();
    int entryVoice = 1;
    LayerIndex voiceLayerIdx = asgn->layer > 0 ? asgn->layer - 1 : 0;
    if (entryInfo) {
        entryVoice = static_cast<int>(entryInfo->getEntry()->voice2) + 1;
        if (asgn->layer == 0) {
            voiceLayerIdx = entryInfo.getLayerIndex();
        }
        if (entryInfo->graceIndex > 0) {
            mnxDynamic.position().set_graceIndex(entryInfo.calcReverseGraceIndex());
        } else if (entryInfo.calcHasGraceNote()) {
            mnxDynamic.position().set_graceIndex(0);
        }
    } else if (context->current.staffMeasureContext) {
        if (const auto measure = context->document->getOthers()->get<others::Measure>(asgn->getRequestedPartId(), context->current.meas)) {
            if (musx::util::Fraction::fromEdu(asgn->eduPosition) >= measure->calcDuration(context->current.staff)) {
                // if we are at the end of a measure, explicitly require main position to ignore any grace
                // notes that might be trailing at the end of the frame
                mnxDynamic.position().set_graceIndex(0);
            }
        }
    }
    mnxDynamic.set_or_clear_placement(mnxMultiStaffPlacementFromVerticalPlacement(mnxStaffNumber, placement));
    if (asgn->layer > 0 || asgn->voice2 || (entryInfo && entryInfo->getEntry()->v2Launch)) {
        mnxDynamic.set_staff(mnxStaffNumber.value_or(1));
        mnxDynamic.set_voice(calcVoice(mnxStaffNumber.value_or(1), voiceLayerIdx, entryVoice));
    } else if (mnxStaffNumber > 1) { // we get better import results not specifying the 1st staff number: this could become an option
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}

void attachFermata(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::MeasureExprAssign>& asgn, const denigma::classify::expression::Fermata& fermataInfo,
    const ExpressionAttachmentContext& attachment, const mnxdom::Fermata& fermata)
{
    if (asgn->calcIsPartOfStaffListAssignment() || fermataInfo.isRightBarline) {
        return;
    }
    if (attachment.entryTarget) {
        switch (attachment.entryTarget->kind) {
        case EntryTargetKind::Event:
            mnxdom::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
            break;
        case EntryTargetKind::FullMeasureRest:
            mnxdom::sequence::FullMeasureRest(context->mnxDocument->root(), attachment.entryTarget->pointer).set_fermata(fermata);
            break;
        }
    } else if (attachment.entryInfo) {
        context->logMessage(
            LogMsg() << "Entry " << attachment.entryInfo->getEntry()->getEntryNumber() << " was not mapped to an event or full measure rest",
            MessageSeverity::Warning);
    } else {
        const int staffNumber = mnxStaffNumber.value_or(1);
        bool attached = false;
        for (auto seq : mnxMeasure.sequences()) {
            if (seq.staff() == staffNumber && seq.content().empty()) {
                seq.ensure_fullMeasure().set_fermata(fermata);
                attached = true;
            }
        }
        if (!attached) {
            auto seq = mnxMeasure.sequences().append();
            seq.set_or_clear_staff(staffNumber);
            seq.ensure_fullMeasure().set_fermata(fermata);
        }
    }
}

void attachBreathMark(const MnxMusxMappingPtr& context, const ExpressionAttachmentContext& attachment, const mnxdom::sequence::BreathMark& breathMark)
{
    if (attachment.entryTarget && attachment.entryTarget->kind == EntryTargetKind::Event) {
        mnxdom::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).ensure_markings().set_breath(breathMark);
    }
};

void attachCaesura(const MnxMusxMappingPtr& context, const ExpressionAttachmentContext& attachment, const mnxdom::sequence::Caesura& caesura)
{
    if (attachment.entryTarget && attachment.entryTarget->kind == EntryTargetKind::Event) {
        mnxdom::sequence::Event(context->mnxDocument->root(), attachment.entryTarget->pointer).ensure_markings().set_caesura(caesura);
    }
}

/// @brief Records a measure repeat counter for the measure, to be attached once the repeats are known.
///
/// MNX gives a part measure one measure repeat shared by every staff, so it has one counter as well.
/// Finale numbers each staff separately, and the staves of a part normally agree; where they do not,
/// only one count can be kept.
void recordMeasureRepeatCount(const MnxMusxMappingPtr& context, const MusxInstance<others::MeasureExprAssign>& asgn,
    const MusxInstance<others::Measure>& musxMeasure, int count, mnxdom::MultiStaffPlacement placement)
{
    // The repeat notation that identifies this expression as a counter can also hide it. A counter
    // Finale does not draw is not counting anything the reader can see.
    if (asgn->calcIsHiddenByAlternateNotation()) {
        return;
    }
    const auto [it, inserted] = context->measureRepeatCounts.emplace(musxMeasure->getCmper(), MnxMusxMapping::MeasureRepeatCount{count, placement});
    if (!inserted && it->second.count != count) {
        context->logMessage(LogMsg() << "Measure repeat counter " << count << " disagrees with counter " << it->second.count
                                     << " on another staff of the same part;"
                                        " MNX has one counter per part measure, so only the first is exported.",
            MessageSeverity::Verbose);
    }
}
} // namespace

void processExpressions(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasExpression) {
        auto exprAssigns =
            context->document->getOthers()->getArray<others::MeasureExprAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : exprAssigns) {
            if (asgn->hidden || asgn->staffAssign != context->current.staff) {
                continue;
            }
            if (!asgn->calcIsAssignedInRequestedPart()) {
                continue;
            }
            const auto classification = classify::classifyExpression(asgn);
            auto placement = asgn->calcVerticalPlacement();
            const bool isCueLayer = asgn->layer > 0 && context->current.cuePlan.isCueLayer(asgn->layer - 1);
            switch (classification.type) {
            case classify::ExpressionType::Dynamic: appendDynamic(context, mnxMeasure, mnxStaffNumber, asgn, classification, placement); break;
            case classify::ExpressionType::Fermata: {
                const auto& fermata = classification.fermata();
                if (auto mnxFermata = makeFermata(fermata.fermata, fermata.glyphStyle, placement)) {
                    attachFermata(context, mnxMeasure, mnxStaffNumber, asgn, fermata, calcAttachmentContext(context, asgn), mnxFermata.value());
                }
                break;
            }
            case classify::ExpressionType::BreathMark:
                attachBreathMark(context, calcAttachmentContext(context, asgn), makeBreathMark(classification.breathMark().breathMark, placement));
                break;
            case classify::ExpressionType::Caesura:
                attachCaesura(context, calcAttachmentContext(context, asgn), makeCaesura(classification.caesura().caesura));
                break;
            case classify::ExpressionType::NonArpeggio: appendArpeggioCandidate(context, mnxMeasure, classification.nonArpeggio().candidate); break;
            case classify::ExpressionType::MeasureRepeatCount:
                recordMeasureRepeatCount(context, asgn, musxMeasure, classification.measureRepeatCount().count,
                    mnxMultiStaffPlacementFromVerticalPlacement(mnxStaffNumber, placement));
                break;
            case classify::ExpressionType::PseudoTie: break;
            case classify::ExpressionType::Error: context->logMessage(LogMsg() << classification.error().message, MessageSeverity::Warning); break;
            // Handled by processGlobalExpressions; see isGlobalExpressionType.
            case classify::ExpressionType::TempoMark: break;
            case classify::ExpressionType::MetronomeMark: break;
            case classify::ExpressionType::TempoAlteration: break;
            case classify::ExpressionType::RehearsalMark: break;
            case classify::ExpressionType::TechniqueText:
            case classify::ExpressionType::GenericText:
            case classify::ExpressionType::StringMute:
            case classify::ExpressionType::HarpDiagram:
            case classify::ExpressionType::AccordionRegistration:
            case classify::ExpressionType::KeyboardPedal:
            case classify::ExpressionType::MultimeasureRestNumber:
                // Until MNX has an object for this expression on a staff, its export is a gap.
                if (auto* const gapCollector = gapCollectorFor(context); gapCollector && !isCueLayer && asgn->textExprId) {
                    gapCollector->add(measureAnchor(mnxMeasure.id_or(""), mnxStaffNumber, asgn->eduPosition), classification);
                }
                break;
            case classify::ExpressionType::Suppress: break;
            }
        }
    }
}

namespace {

/// @brief One tempo MNX will write for a position, with the source it came from.
struct TempoCandidate
{
    mnxdom::global::Tempo::Required tempo;
    std::string id;
    /// @brief The expression group the tempo came from.
    const classify::ExpressionAssignmentGroup* group{};
};

/// @brief Collects the tempo an expression group plays, if it plays one.
///
/// An MNX tempo is a playback instruction, so it takes the expression's playback settings; a
/// metronome mark without them falls back to its displayed equation. See design-decisions.md.
std::optional<mnxdom::global::Tempo::Required> calcExpressionTempo(const classify::ExpressionClassification& classification)
{
    const auto* tempoText = classification.as<classify::expression::TempoText>();
    const auto* metronomeMark = classification.as<classify::expression::MetronomeMark>();
    const classify::expression::TempoInfo* playback = nullptr;
    if (metronomeMark) {
        playback = &metronomeMark->tempo;
    } else if (tempoText) {
        playback = &tempoText->tempo;
    }
    if (playback && playback->beatsPerMinute > 0 && playback->beatUnitEdu > 0) {
        return mnxTempoFromPlayback(playback->beatsPerMinute, Edu(playback->beatUnitEdu));
    }
    if (metronomeMark) {
        return mnxTempoFromMetronomeMark(*metronomeMark);
    }
    return std::nullopt;
}

/// @brief Writes the tempos the measure's expressions play, one per position, in position order,
/// and records what each expression lost on the way (see design-decisions.md).
void appendTempos(
    const MnxMusxMappingPtr& context, mnxdom::global::Measure& mnxMeasure, const std::vector<classify::ExpressionAssignmentGroup>& groups)
{
    auto* const gapCollector = gapCollectorFor(context);
    const auto globalMeasureId = mnxMeasure.id_or("");
    std::map<Edu, TempoCandidate> temposAtPositions;

    // Search in order of decreasing precedence: text expressions before shape expressions. Using
    // emplace keeps the first tempo at a position.
    const auto addExpressionTempos = [&](bool textExpressions) {
        for (const auto& group : groups) {
            const auto& primary = group.primary;
            if (!isTempoFamily(group.classification.type) || static_cast<bool>(primary->textExprId) != textExpressions) {
                continue;
            }
            const auto tempo = calcExpressionTempo(group.classification);
            const bool emitted =
                tempo && temposAtPositions.emplace(primary->eduPosition, TempoCandidate{*tempo, core::calcExpressionId(primary), &group}).second;
            if (emitted || !gapCollector || !textExpressions) {
                continue;
            }
            gapCollector->add(measureAnchor(globalMeasureId, std::nullopt, primary->eduPosition), group.classification, classify::GapExtent::Complete,
                placementsForGroup(context, group, globalMeasureId));
        }
    };
    addExpressionTempos(true);
    addExpressionTempos(false);

    for (const auto& [position, candidate] : temposAtPositions) {
        auto tempo = mnxMeasure.ensure_tempos().append(candidate.tempo.bpm, candidate.tempo.noteValue);
        tempo.set_id(candidate.id);
        if (position) {
            tempo.ensure_location(mnxFractionFromFraction(Fraction::fromEdu(position)));
        }
        if (!gapCollector) {
            continue;
        }
        const classify::GapAnchor anchor{candidate.id, std::nullopt, std::nullopt};
        if (candidate.group->primary->hidden) {
            /// @todo hide hidden expressions if MNX ever adds visibility to the tempo object.
            gapCollector->add(anchor, classify::PlaybackOnly{}, classify::GapExtent::Partial);
        }
        // Reported whether or not Finale draws it; see design-decisions.md.
        if (candidate.group->primary->textExprId) {
            gapCollector->add(anchor, candidate.group->classification, classify::GapExtent::Partial,
                placementsForGroup(context, *candidate.group, globalMeasureId));
        }
    }
}

} // namespace

void processGlobalExpressions(const MnxMusxMappingPtr& context, mnxdom::global::Measure& mnxMeasure, const MusxInstance<others::Measure>& musxMeasure)
{
    if (!musxMeasure->hasExpression) {
        return;
    }
    const auto groups = classify::groupExpressionAssignments(
        musxMeasure->getDocument()->getOthers()->getArray<others::MeasureExprAssign>(SCORE_PARTID, musxMeasure->getCmper()));
    appendTempos(context, mnxMeasure, groups);

    auto* const gapCollector = gapCollectorFor(context);
    const auto globalMeasureId = mnxMeasure.id_or("");
    for (const auto& group : groups) {
        const auto& primary = group.primary;
        const auto& classification = group.classification;
        if (!primary->textExprId || primary->hidden || !isGlobalExpressionType(classification.type)) {
            continue;
        }
        switch (classification.type) {
        case classify::ExpressionType::TempoMark: break; // appendTempos
        case classify::ExpressionType::MetronomeMark: break; // appendTempos
        case classify::ExpressionType::TempoAlteration:
        case classify::ExpressionType::RehearsalMark:
            // Until MNX has an object for this marking, its export is a gap.
            if (gapCollector) {
                gapCollector->add(measureAnchor(globalMeasureId, std::nullopt, primary->eduPosition), classification, classify::GapExtent::Complete,
                    placementsForGroup(context, group, globalMeasureId));
            }
            break;
        default: break; // not a global type; see isGlobalExpressionType
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
