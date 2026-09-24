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

#include <type_traits>

#include "core/element_ids.h"
#include "denigma/classify/smartshapes.h"
#include "mnx.h"
#include "mnx_articulations.h"
#include "mnx_gaps.h"
#include "mnx_smartshapes.h"

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

namespace {
void appendHairpin(const MnxMusxMappingPtr&, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::SmartShape>& shape, mnxdom::DynamicWedgeType wedgeType)
{
    const auto startPos = mnxFractionFromFraction(shape->startTermSeg->endPoint->calcGlobalPosition());
    const auto endPos = mnxdom::MeasureRhythmicPosition::make(core::calcGlobalMeasureId(shape->endTermSeg->endPoint->calcMeasure()),
        mnxFractionFromFraction(shape->endTermSeg->endPoint->calcGlobalPosition()));
    auto mnxDynamic = mnxMeasure.ensure_dynamics().appendGradual(wedgeType, startPos, endPos);
    /// @todo Perhaps get smarter about setting start/end grace index using situational heuristics
    mnxDynamic.position().set_graceIndex(0);        // always after grace notes
    mnxDynamic.end().position().set_graceIndex(0);  // always after grace notes
    mnxDynamic.set_or_clear_placement(mnxMultiStaffPlacementFromVerticalPlacement(mnxStaffNumber, shape->calcVerticalPlacementForBeatAttached()));
    if (mnxStaffNumber > 1) { // we get better import results not specifying the 1st staff number: this could become an option
        mnxDynamic.set_staff(mnxStaffNumber.value());
    }
}

/// @brief A hidden shape draws nothing, so losing it loses nothing.
bool isReportable(const MusxInstance<others::SmartShape>& shape)
{
    return !shape->hidden;
}

/// @brief The anchor of a shape endpoint that reaches a part measure: the measure, staff and position.
std::optional<classify::GapAnchor> endPointAnchor(const MnxMusxMappingPtr& context, const MusxInstance<smartshape::EndPoint>& endPoint)
{
    return partMeasureAnchor(context, endPoint->calcStaff(), endPoint->calcMeasure(), endPoint->calcGlobalPosition());
}

/// @brief Records the gap for a beat-attached shape MNX does not export, spanning its endpoints.
void addMeasureShapeGap(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::SmartShape>& shape, const classify::SmartShapeClassification& classification)
{
    auto* const gapCollector = gapCollectorFor(context);
    if (!gapCollector || !isReportable(shape)) {
        return;
    }
    classify::GapAnchor start{mnxMeasure.id_or(""), mnxStaffNumber, shape->startTermSeg->endPoint->calcGlobalPosition()};
    if (auto end = endPointAnchor(context, shape->endTermSeg->endPoint)) {
        gapCollector->addSpan(std::move(start), std::move(*end), classification);
    } else {
        gapCollector->add(std::move(start), classification);
    }
}
} // namespace

void processSmartShapes(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber)
{
    if (musxMeasure->hasSmartShape) {
        const auto assigns =
            context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& assign : assigns) {
            ASSERT_IF (!assign) {
                context->logMessage(
                    LogMsg() << "skipping empty smart shape assignment for measure " << musxMeasure->getCmper(), MessageSeverity::Warning);
                continue;
            }
            if (assign->centerShapeNum != 0) {
                // ignore assignments in the middle of the shape
                continue;
            }
            const auto shape = context->document->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
            if (!shape || !shape->calcIsValid()) {
                continue;
            }
            if (shape->entryBased) {
                // Entry-attached shapes are exported or reported from their start entry. (See processEntrySmartShapes.)
                continue;
            }
            // The recorded staff and measure, not calcStaff/calcMeasure: this is the measure whose
            // assignment names the shape, and Finale keeps the assignment where the endpoint was recorded.
            if (shape->startTermSeg->endPoint->staffId != context->current.staff
                || shape->startTermSeg->endPoint->measId != musxMeasure->getCmper()) {
                continue;
            }
            const auto classification = denigma::classify::classifySmartShape(shape);
            std::visit(
                [&](const auto& value) {
                    using Value = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<Value, denigma::classify::smartshape::Crescendo>) {
                        appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnxdom::DynamicWedgeType::Increasing);
                    } else if constexpr (std::is_same_v<Value, denigma::classify::smartshape::Decrescendo>) {
                        appendHairpin(context, mnxMeasure, mnxStaffNumber, shape, mnxdom::DynamicWedgeType::Decreasing);
                    } else if constexpr (std::is_same_v<Value, denigma::classify::smartshape::NonArpeggio>) {
                        appendArpeggioCandidate(context, mnxMeasure, value.candidate);
                    } else if constexpr (std::is_same_v<Value, denigma::classify::smartshape::Ottava>
                                         || std::is_same_v<Value, denigma::classify::PseudoTie>
                                         || std::is_same_v<Value, denigma::classify::smartshape::ArpeggiatedTie>
                                         || std::is_same_v<Value, denigma::classify::smartshape::Suppress>) {
                        // Processed by the dedicated ottava, note-level tie, or lyric paths. A visual proxy of a
                        // hidden ottava is represented by the ottava its carrier exports.
                    } else {
                        // Until MNX has an object for this shape, its export is a gap. A beat-attached slur is
                        // one of them: the slur path hosts only entry-attached slurs.
                        addMeasureShapeGap(context, mnxMeasure, mnxStaffNumber, shape, classification);
                    }
                },
                classification.value);
        }
    }
}

void processEntrySmartShapes(const MnxMusxMappingPtr& context, mnxdom::sequence::Event& mnxEvent, const EntryInfoPtr& musxEntryInfo)
{
    const auto musxEntry = musxEntryInfo->getEntry();
    const auto currentEntryNumber = musxEntry->getEntryNumber();
    if (!musxEntry->smartShapeDetail) {
        return;
    }
    // The end entry may not be exported (a cue layer, for instance), which is known only once every
    // entry has been; finalizeEntryTargets then removes the slur and reports it as a gap.
    auto createOneSlur = [&](const MusxInstance<others::SmartShape>& shape, const classify::SmartShapeClassification& classification,
                             const EntryInfoPtr& targetEntry) -> mnxdom::sequence::Slur {
        auto mnxSlurs = mnxEvent.ensure_slurs();
        auto mnxSlur = mnxSlurs.append(core::calcEventId(targetEntry));
        context->deferredSlurTargets.push_back({{mnxSlur.pointer(), targetEntry->getEntry()->getEntryNumber()}, shape, classification});
        return mnxSlur;
    };
    // A shape that MNX does not export is reported from its start entry, anchored to the note it
    // starts from when it names one and to the event otherwise; its end is resolved in finalizeSmartShapeGaps.
    auto deferGap = [&](const MusxInstance<others::SmartShape>& shape, const classify::SmartShapeClassification& classification) {
        if (!gapCollectorFor(context) || !isReportable(shape)) {
            return;
        }
        const auto startNote = shape->calcStartNote();
        classify::GapAnchor start{startNote ? core::calcNoteId(startNote) : core::calcEventId(musxEntryInfo), std::nullopt, std::nullopt};
        context->deferredSmartShapeGaps.push_back({shape, classification, std::move(start)});
    };
    auto shapeAssigns = musxEntry->getDocument()->getDetails()->getArray<details::SmartShapeEntryAssign>(SCORE_PARTID, musxEntry->getEntryNumber());
    for (const auto& assign : shapeAssigns) {
        const auto shape = musxEntry->getDocument()->getOthers()->get<others::SmartShape>(SCORE_PARTID, assign->shapeNum);
        if (!shape || !shape->calcIsValid()) {
            continue;
        }
        if (shape->startTermSeg->endPoint->entryNumber != currentEntryNumber) {
            // The end entry carries an assignment too; the shape belongs to its start.
            continue;
        }
        const auto classification = denigma::classify::classifySmartShape(shape);
        if (classification.as<denigma::classify::PseudoTie>() || classification.as<denigma::classify::smartshape::ArpeggiatedTie>()
            || classification.as<denigma::classify::smartshape::Suppress>()) {
            // Handled by the note-level tie or lyric paths.
            continue;
        }
        const auto* slur = classification.as<denigma::classify::smartshape::Slur>();
        if (!slur) {
            deferGap(shape, classification);
            continue;
        }
        if (!slur->startEntry || !slur->endEntry || slur->endEntry->getEntry()->getEntryNumber() == currentEntryNumber) {
            // MNX slurs run between two events, so a slur with a floating or coinciding endpoint is a gap.
            deferGap(shape, classification);
            continue;
        }
        auto mnxSlur = createOneSlur(shape, classification, slur->endEntry);
        mnxSlur.set_lineType(shape->calcIsDashed() ? mnxdom::LineType::Dashed : mnxdom::LineType::Solid);
        if (slur->contour != CurveContourDirection::Unspecified) {
            mnxSlur.set_or_clear_side(slur->contour == CurveContourDirection::Up ? mnxdom::SlurTieSide::Up : mnxdom::SlurTieSide::Down);
        }
    }
}

void finalizeSmartShapeGaps(const MnxMusxMappingPtr& context)
{
    auto* const gapCollector = gapCollectorFor(context);
    if (!gapCollector) {
        context->deferredSmartShapeGaps.clear();
        return;
    }
    for (auto& deferred : context->deferredSmartShapeGaps) {
        const auto& endPoint = deferred.shape->endTermSeg->endPoint;
        std::optional<classify::GapAnchor> end;
        const auto targetIt = context->entryTargetByNumber.find(endPoint->entryNumber);
        if (targetIt != context->entryTargetByNumber.end() && targetIt->second.kind == EntryTargetKind::Event) {
            const auto endNote = deferred.shape->calcEndNote();
            const auto endEntryInfo = endPoint->calcAssociatedEntry();
            end = classify::GapAnchor{endNote ? core::calcNoteId(endNote) : core::calcEventId(endEntryInfo), std::nullopt, std::nullopt};
        } else {
            // The end entry was not exported (a cue layer, or a full-measure rest with no event id), so
            // the end names the measure it falls in.
            end = endPointAnchor(context, endPoint);
        }
        if (end) {
            gapCollector->addSpan(std::move(deferred.start), std::move(*end), std::move(deferred.classification));
        } else {
            gapCollector->add(std::move(deferred.start), std::move(deferred.classification));
        }
    }
    context->deferredSmartShapeGaps.clear();
}

void createOttavas(const MnxMusxMappingPtr& context, const MusxInstance<others::Measure>& musxMeasure, mnxdom::part::Measure& mnxMeasure,
    std::optional<int> mnxStaffNumber)
{
    const StaffCmper staffCmper = context->current.staff;
    context->current.ottavasApplicableInMeasure =
        collectOttavasForMeasureStaff(context->document, musxMeasure->getRequestedPartId(), musxMeasure, staffCmper);
    if (musxMeasure->hasSmartShape) {
        auto shapeAssigns =
            context->document->getOthers()->getArray<others::SmartShapeMeasureAssign>(musxMeasure->getRequestedPartId(), musxMeasure->getCmper());
        for (const auto& asgn : shapeAssigns) {
            if (auto shape = context->document->getOthers()->get<others::SmartShape>(asgn->getRequestedPartId(), asgn->shapeNum)) {
                const auto it = context->current.ottavasApplicableInMeasure.find(shape->getCmper());
                if (it != context->current.ottavasApplicableInMeasure.end()) {
                    // The recorded measure: it is the one whose assignment names the shape.
                    if (!asgn->centerShapeNum && shape->startTermSeg->endPoint->measId == musxMeasure->getCmper()) {
                        // Semantic carriers are emitted even when hidden: a hidden built-in
                        // ottava carries the octave displacement for its visual proxy.
                        auto mnxOttava = mnxMeasure.ensure_ottavas().append(static_cast<mnxdom::OttavaAmount>(it->second.classification.octaveShift),
                            mnxFractionFromSmartShapeEndPoint(shape->startTermSeg->endPoint),
                            mnxdom::MeasureRhythmicPosition::make(core::calcGlobalMeasureId(shape->endTermSeg->endPoint->calcMeasure()),
                                mnxFractionFromSmartShapeEndPoint(shape->endTermSeg->endPoint)));
                        mnxOttava.end().position().set_graceIndex(0);   // guarantees inclusion of any grace notes at the end of the ottava
                        if (mnxStaffNumber) {
                            mnxOttava.set_staff(mnxStaffNumber.value());
                        }
                    }
                }
            }
        }
    }
}

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
