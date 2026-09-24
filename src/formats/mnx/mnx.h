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
#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/cue_plan.h"
#include "core/denigma.h"
#include "core/finale_options.h"
#include "core/ottavas.h"
#include "mnxdom.h"
#include "musx/musx.h"

#include "denigma/classify/gaps.h"
#include "denigma/classify/jumps.h"
#include "mnx_articulations.h"
#include "mnx_fwd.h"
#include "mnx_mapping.h"

using namespace musx::dom;
using namespace musx::util;

namespace denigma {
namespace formats {
namespace mnx {
namespace detail {

enum class EntryTargetKind {
    Event,
    FullMeasureRest
};

struct EntryTarget
{
    EntryTargetKind kind;
    mnxdom::json_pointer pointer;
};

// stoopid c++17 standard does not include a hash for tuple
struct SequenceHash
{
    std::size_t operator()(const std::tuple<StaffCmper, LayerIndex, int>& t) const
    {
        auto [x, y, z] = t;
        std::size_t h1 = std::hash<int>{}(x);
        std::size_t h2 = std::hash<int>{}(y);
        std::size_t h3 = std::hash<int>{}(z);
        return h1 ^ (h2 << 1) ^ (h3 << 2); // XOR-shift for better hash distribution
    }
};

using json = nlohmann::ordered_json;
//using json = nlohmann::json;

struct MnxMusxMapping
{
    MnxMusxMapping(const DenigmaContext& context, const DocumentPtr& doc)
        : denigmaContext(&context), document(doc), finaleOptions(loadFinaleOptions(doc, SCORE_PARTID)), mnxDocument(), musxParts(doc, SCORE_PARTID)
    {}

    const DenigmaContext* denigmaContext;
    musx::dom::DocumentPtr document;
    FinaleOptions finaleOptions;
    std::unique_ptr<mnxdom::Document> mnxDocument;
    MusxInstanceList<others::PartDefinition> musxParts;

    std::unordered_map<std::string, std::vector<StaffCmper>> part2Inst;
    std::unordered_map<StaffCmper, std::string> inst2Part;
    std::unordered_map<std::string, std::string> part2SplitInstrumentUuid;
    std::unordered_set<std::string> lyricLineIds;

    // musx mappings
    std::unordered_map<std::string, mnxdom::json_pointer> noteJsonById;
    std::unordered_map<EntryNumber, EntryTarget> entryTargetByNumber;

    struct DeferredJumpTie
    {
        std::string startNoteId;
        std::string endNoteId;
        mnxdom::SlurTieSide side{mnxdom::SlurTieSide::Auto};
    };

    std::vector<DeferredJumpTie> deferredJumpTies;
    std::unordered_set<std::string> deferredJumpTieKeys;
    std::vector<musx::util::ArpeggioSpanCandidate> deferredArpeggios;
    std::unordered_set<std::string> deferredArpeggioKeys;

    /// @brief An entry-attached smart shape MNX does not export, held until every entry has been
    /// exported so its end can be anchored to an exported event or, failing that, a measure.
    struct DeferredSmartShapeGap
    {
        MusxInstance<others::SmartShape> shape;
        classify::SmartShapeClassification classification;
        classify::GapAnchor start;
    };

    std::vector<DeferredSmartShapeGap> deferredSmartShapeGaps;

    /// @brief A lyric word extension, held until every entry has been exported so its end can be
    /// anchored to an exported event or, failing that, a measure.
    struct DeferredLyricExtensionGap
    {
        classify::LyricWordExtension classification;
        classify::GapAnchor start;
    };

    std::vector<DeferredLyricExtensionGap> deferredLyricExtensionGaps;

    /// @brief A tie or slur written against an entry that may not be exported (a cue layer, for
    /// instance), held until every entry has been exported so the reference can be checked.
    struct DeferredEntryTarget
    {
        mnxdom::json_pointer pointer; ///< The tie or slur in the MNX document.
        EntryNumber targetEntry; ///< The entry the target id names.
    };

    std::vector<DeferredEntryTarget> deferredTieTargets;

    /// @brief A slur, with the shape it came from so a slur whose end was never exported can be reported
    /// as a gap instead.
    struct DeferredSlurTarget
    {
        DeferredEntryTarget target;
        MusxInstance<others::SmartShape> shape;
        classify::SmartShapeClassification classification;
    };

    std::vector<DeferredSlurTarget> deferredSlurTargets;

    std::optional<std::string> currSplitInstrumentUuid;
    std::vector<StaffCmper> currPartStaves;
    std::unordered_set<EntryNumber> beamedEntries;
    size_t discardedCueFrames{};
    /// @brief Zero-length tuplets whose entries were omitted; see addEntryToContent.
    size_t discardedZeroLengthTuplets{};

    /// @brief Measure repeat counts found on the current part's staves, keyed by measure.
    ///
    /// Collected while the staves are exported and consumed once the measure repeats themselves are
    /// known, because MNX attaches a counter to the repeat rather than to the measure.
    struct MeasureRepeatCount
    {
        int count{};
        mnxdom::MultiStaffPlacement placement{mnxdom::MultiStaffPlacement::Auto};
    };
    std::map<MeasCmper, MeasureRepeatCount> measureRepeatCounts;

    struct CurrentMeasureStaff
    {
        MeasCmper meas{};
        StaffCmper staff{};
        std::string voice;
        std::optional<details::GFrameHoldContext> staffMeasureContext;
        std::map<LayerIndex, int> layerVoices;
        /// @brief Layers whose entries Finale replaces with alternate notation, so they are not exported.
        std::unordered_set<LayerIndex> layersHiddenByAltNotation;
        CueStaffMeasurePlan cuePlan;
        OttavaShapeMap ottavasApplicableInMeasure;

        void clear()
        {
            meas = staff = 0;
            voice.clear();
            staffMeasureContext.reset();
            layerVoices.clear();
            layersHiddenByAltNotation.clear();
            cuePlan = {};
            ottavasApplicableInMeasure.clear();
        }
    };

    CurrentMeasureStaff current;

    std::optional<int> mnxPartStaffFromStaff(StaffCmper staff) const;

    void clearCounts()
    {
        currSplitInstrumentUuid.reset();
        currPartStaves.clear();
        beamedEntries.clear();
        measureRepeatCounts.clear();
        current.clear();
    }

    void setCurrentMeasureStaff(const MusxInstance<others::Measure>& musxMeasure, StaffCmper staffCmper);
    void logMessage(LogMsg&& msg, MessageSeverity severity = MessageSeverity::Info);
    void logDiscardedHeuristicCueStaffMeasure();
    void logDiscardedCueLayerFrame(LayerIndex layer);
};

std::string mnxPartDisplayName(const MnxMusxMappingPtr& context, const std::string& partId);
std::string mnxPartDisplayName(const MnxMusxMappingPtr& context, const mnxdom::Part& part);
std::string mnxPartDisplayList(const MnxMusxMappingPtr& context, const std::vector<std::string>& partIds);

inline std::string calcSystemLayoutId(const MusxInstance<others::PartDefinition>& linkedPart, Cmper systemId)
{
    const Cmper partId = linkedPart->getCmper();
    if (linkedPart->getDocument()->isScrollViewCmper(partId, systemId)) {
        return "S" + std::to_string(partId) + "-ScrVw";
    }
    return "S" + std::to_string(partId) + "-Sys" + std::to_string(systemId);
}

inline std::string calcVoice(int partStaffNum, LayerIndex idx, int voice)
{
    std::string result = "s" + std::to_string(partStaffNum) + "layer" + std::to_string(idx + 1);
    if (voice > 1) {
        result += "v" + std::to_string(voice);
    }
    return result;
}

inline std::string calcLyricLineId(const std::string& type, Cmper textNumber)
{
    return type.substr(0, 1) + std::to_string(textNumber);
}

inline std::string calcPercussionKitId(const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    return "ke" + std::to_string(percNoteInfo->percNoteType);
}

inline std::string calcPercussionSoundId(const MusxInstance<others::PercussionNoteInfo>& percNoteInfo)
{
    std::string result = "pn" + std::to_string(percNoteInfo->getBaseNoteTypeId());
    if (auto orderId = percNoteInfo->getNoteTypeOrderId()) {
        result += "o" + std::to_string(orderId + 1);
    }
    return result;
}

void createLayouts(const MnxMusxMappingPtr& context);
void createGlobal(const MnxMusxMappingPtr& context);
void createParts(const MnxMusxMappingPtr& context);
void createSequences(const MnxMusxMappingPtr& context, mnxdom::part::Measure& mnxMeasure, std::optional<int> mnxStaffNumber,
    const MusxInstance<others::Measure>& musxMeasure);
void finalizeJumpTies(const MnxMusxMappingPtr& context);
void finalizeLyricExtensionGaps(const MnxMusxMappingPtr& context);
/// @brief Resolves ties and slurs whose target entry was never exported: a tie becomes l.v. and a slur is
/// removed. Must run after every entry has been exported and before any pass that reads the ties.
void finalizeEntryTargets(const MnxMusxMappingPtr& context);

/// @brief Whether the tuplet is a zero-length tuplet whose entries MNX omits.
///
/// A zero-length tuplet is a Finale workaround (a phantom note that extends a beam over a barline, for
/// instance) whose entries occupy no time. An MNX tuplet ratio must be positive, and an event outside
/// one always takes its written duration, so the entries have no representation. The singleton-beam
/// form is excluded: the interpreted iterator resolves it. (See EntryFrame::TupletInfo::calcCreatesSingleton.)
bool isOmittedZeroLengthTuplet(const EntryFrame::TupletInfo& tupletInfo);
/// @brief Whether the entry lies in a tuplet #isOmittedZeroLengthTuplet omits, so nothing may refer to it.
bool isInOmittedZeroLengthTuplet(const EntryInfoPtr& entryInfo);

void exportJson(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext);
void exportJson(std::ostream& output, const CommandInputData& inputData, const DenigmaContext& denigmaContext);
void exportMnx(const std::filesystem::path& outputPath, const CommandInputData& inputData, const DenigmaContext& denigmaContext);

template <typename ToEnum, typename FromEnum>
ToEnum enumConvert(FromEnum value);

} // namespace detail
} // namespace mnx
} // namespace formats
} // namespace denigma
