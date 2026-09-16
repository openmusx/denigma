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
#pragma once

#include <string>
#include <utility>

#include "denigma/classify/gaps.h"

namespace denigma {

/// @struct GapReportProducer
/// @brief Identifies the software that serialized a gap report.
struct GapReportProducer
{
    std::string name;
    std::string version;
    std::string commit;
};

#if DENIGMA_HAS_GAP_REPORT
/// True when this build includes the gap report serializer (`denigma::gap-report`).
inline constexpr bool GAP_REPORT_AVAILABLE = true;

/// Serializes a collector as JSON, including an empty `gaps` array when no gaps were collected.
std::string serializeGapReport(const classify::GapCollector& collector, const GapReportProducer& producer);

/// @class GapReportWriter
/// @brief Serialization handle passed to a #withGapReport callback.
class GapReportWriter
{
public:
    /// @brief Wraps the collector whose gaps the callback serializes.
    explicit GapReportWriter(const classify::GapCollector& collector)
        : m_collector(collector)
    {}

    /// @brief Serializes the wrapped collector as JSON. (See #serializeGapReport.)
    [[nodiscard]] std::string serialize(const GapReportProducer& producer) const { return serializeGapReport(m_collector, producer); }

private:
    const classify::GapCollector& m_collector;
};
#else
inline constexpr bool GAP_REPORT_AVAILABLE = false;

/// @class GapReportWriter
/// @brief Placeholder in builds without the serializer. #withGapReport never passes it to a callback.
class GapReportWriter
{
public:
    explicit GapReportWriter(const classify::GapCollector&) {}
};
#endif // DENIGMA_HAS_GAP_REPORT

/// @brief Invokes a generic serialization callback synchronously, only in builds that include the gap report serializer.
/// @details The callback receives a #GapReportWriter for @p collector. Write it as a generic lambda whose body depends
/// on the writer parameter, so that a build without the serializer never instantiates it and needs neither the
/// serializer's symbols nor its headers.
/// @return True when the callback ran, false when this build has no gap report serializer.
template <typename Callback>
bool withGapReport(const classify::GapCollector& collector, Callback&& callback)
{
    if constexpr (GAP_REPORT_AVAILABLE) {
        GapReportWriter writer(collector);
        std::forward<Callback>(callback)(writer);
    }
    return GAP_REPORT_AVAILABLE;
}

} // namespace denigma
