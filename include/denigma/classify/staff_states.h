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
#pragma once

#include <functional>

#include "musx/musx.h"

namespace denigma {
namespace classify {

namespace staff_state {

/// @struct Transposition
/// @brief Written-to-concert transposition interval of a staff.
struct Transposition
{
    /// Diatonic displacement, as returned by `others::Staff::calcTranspositionInterval`.
    int displacement{};
    /// Chromatic alteration, as returned by `others::Staff::calcTranspositionInterval`.
    int alteration{};

    bool operator==(const Transposition&) const = default;
};

} // namespace staff_state

/// @struct StaffState
/// @brief Effective values of the staff settings that staff styles and systems can change.
///
/// A default-constructed instance holds Finale's defaults.
struct StaffState
{
    /// Number of staff lines. Zero means the staff draws no lines.
    int numberOfLines{music_theory::STANDARD_NUMBER_OF_STAFFLINES};
    /// Distance between staff lines.
    musx::dom::Evpu lineSpace{static_cast<musx::dom::Evpu>(musx::dom::EVPU_PER_SPACE)};
    /// Staff scaling of the system containing the point, from its staff list. It is 1 when the
    /// part's layout is not calculated.
    musx::util::Fraction staffScaling{1};
    /// Transposition interval.
    staff_state::Transposition transposition{};

    bool operator==(const StaffState&) const = default;
};

/// @struct StaffStateChange
/// @brief One point at which the effective staff state changes.
struct StaffStateChange
{
    /// Location of the change, in staff-level time.
    musx::dom::MusicPoint point;
    /// State before the change: the previously reported state, or the caller's baseline.
    StaffState previous;
    /// State from #point onward. At least one member differs from #previous.
    StaffState current;
    /// Effective staff at #point.
    musx::dom::MusxInstance<musx::dom::others::StaffComposite> staff;
};

/// Calculates the effective staff state at a point.
/// @param document The source document.
/// @param partId The part whose staff styles and systems apply.
/// @param staffId The staff.
/// @param point The location, in staff-level time.
/// @return The state.
/// @throws std::invalid_argument if @p partId has no staff @p staffId.
StaffState calcStaffState(
    const musx::dom::DocumentPtr& document, musx::dom::Cmper partId, musx::dom::StaffCmper staffId, const musx::dom::MusicPoint& point);

/// Reports each point, in order, at which the effective state of a staff changes.
///
/// Staff state can change only at the start of the document, where a staff style begins or ends,
/// and, when the part's layout is calculated, at the start of a system that contains the staff.
/// @param document The source document.
/// @param partId The part whose staff styles and systems apply.
/// @param staffId The staff.
/// @param baseline State the caller already holds before the first measure. The start of the
/// document is reported only when it differs from this.
/// @param callback Called once per change. Return false to stop the walk.
/// @return True if every change was reported. False if @p callback stopped the walk.
/// @throws std::invalid_argument if @p partId has no staff @p staffId.
bool iterateStaffStateChanges(const musx::dom::DocumentPtr& document, musx::dom::Cmper partId, musx::dom::StaffCmper staffId,
    const StaffState& baseline, const std::function<bool(const StaffStateChange&)>& callback);

} // namespace classify
} // namespace denigma
