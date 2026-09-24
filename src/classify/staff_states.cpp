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
#include "denigma/classify/staff_states.h"

#include <optional>
#include <set>
#include <stdexcept>
#include <string>

#include "core/denigma.h"

namespace denigma {
namespace classify {

using namespace musx::dom;
using musx::util::Fraction;

namespace {

/// Evaluates staff state for one staff, caching what every point of a walk shares.
class StaffStateEvaluator
{
public:
    StaffStateEvaluator(const DocumentPtr& document, Cmper partId, StaffCmper staffId)
        : m_document(document), m_partId(partId), m_staffId(staffId)
    {
        const auto part = document->getOthers()->get<others::PartDefinition>(SCORE_PARTID, partId);
        if (part && part->isLayoutCalculated()) {
            m_systems = document->getOthers()->getArray<others::StaffSystem>(partId);
        }
        m_lastMeasure = static_cast<MeasCmper>(document->getOthers()->getArray<others::Measure>(partId).size());
    }

    /// Systems of the part, or std::nullopt when its layout is not calculated. An uncalculated
    /// layout leaves zero-valued system start measures that locate nothing.
    const std::optional<MusxInstanceList<others::StaffSystem>>& systems() const { return m_systems; }

    MeasCmper lastMeasure() const { return m_lastMeasure; }

    /// Fills @p state for @p point and returns the effective staff there.
    /// @p fallbackScaling stands in when a calculated layout has no system for the point.
    /// @throws std::invalid_argument if the part has no such staff.
    MusxInstance<others::StaffComposite> evaluate(const MusicPoint& point, const Fraction& fallbackScaling, StaffState& state) const
    {
        auto staff = others::StaffComposite::createCurrent(m_document, m_partId, m_staffId, point.measureId, point.position.calcEduDuration());
        // createCurrent fails only when the staff itself is missing, which no point of a walk can cause.
        ASSERT_IF (!staff) {
            throw std::invalid_argument("Staff " + std::to_string(m_staffId) + " does not exist in part " + std::to_string(m_partId) + ".");
        }
        state.numberOfLines = staff->calcNumberOfStafflines();
        state.lineSpace = staff->lineSpace;
        const auto [displacement, alteration] = staff->calcTranspositionInterval();
        state.transposition = {displacement, alteration};
        state.staffScaling = Fraction{1};
        if (m_systems) {
            const auto system = m_document->calcSystemFromMeasure(m_partId, point.measureId);
            state.staffScaling = system ? system->calcStaffScaling(m_staffId) : fallbackScaling;
        }
        return staff;
    }

private:
    DocumentPtr m_document;
    Cmper m_partId{};
    StaffCmper m_staffId{};
    std::optional<MusxInstanceList<others::StaffSystem>> m_systems;
    MeasCmper m_lastMeasure{};
};

} // namespace

StaffState calcStaffState(const DocumentPtr& document, Cmper partId, StaffCmper staffId, const MusicPoint& point)
{
    const StaffStateEvaluator evaluator(document, partId, staffId);
    StaffState result;
    evaluator.evaluate(point, Fraction{1}, result);
    return result;
}

bool iterateStaffStateChanges(const DocumentPtr& document, Cmper partId, StaffCmper staffId, const StaffState& baseline,
    const std::function<bool(const StaffStateChange&)>& callback)
{
    const StaffStateEvaluator evaluator(document, partId, staffId);

    std::set<MusicPoint> points{MusicPoint{}};
    if (const auto rawStaff = document->getOthers()->get<others::Staff>(partId, staffId); rawStaff && rawStaff->hasStyles) {
        for (const auto& styleAssign : document->getOthers()->getArray<others::StaffStyleAssign>(partId, staffId)) {
            points.emplace(styleAssign->startMeas, Fraction::fromEdu(styleAssign->startEdu));
            if (const auto nextLocation = styleAssign->nextLocation(staffId)) {
                points.emplace(*nextLocation);
            }
        }
    }
    if (const auto& systems = evaluator.systems()) {
        for (const auto& system : *systems) {
            const auto systemStaves = document->getOthers()->getArray<others::StaffUsed>(partId, system->getCmper());
            if (systemStaves.getIndexForStaff(staffId).has_value()) {
                points.emplace(system->startMeas, Fraction{});
            }
        }
    }

    StaffState previous = baseline;
    for (const auto& point : points) {
        if (point.measureId <= 0 || point.measureId > evaluator.lastMeasure()) {
            continue;
        }
        StaffState current;
        const auto staff = evaluator.evaluate(point, previous.staffScaling, current);
        if (current == previous) {
            continue;
        }
        if (!callback(StaffStateChange{point, previous, current, staff})) {
            return false;
        }
        previous = current;
    }
    return true;
}

} // namespace classify
} // namespace denigma
