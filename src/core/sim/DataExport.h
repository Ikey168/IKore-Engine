#pragma once

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

/**
 * @file DataExport.h
 * @brief Per-tick simulation data export to CSV / JSON (issue #155).
 *
 * Milestone 12. Streams per-tick agent/world state to a structured, parseable
 * file for offline analysis. The exporter writes to any std::ostream (a
 * std::ofstream for a file, or a std::ostringstream in tests), one row per
 * record:
 *   - CSV: a header row of column names, then one comma-separated row per record.
 *   - JSON: a single array of objects, each mapping column name -> value, so the
 *     whole output is one valid JSON document.
 *
 * Values are numeric (the common case for agent state: tick, id, x, y, z, ...).
 * Integral values are written without a decimal point so ids and ticks stay
 * readable; the rest use compact round-trippable formatting.
 *
 * Negligible cost when disabled: a default-constructed exporter (or one never
 * given a stream) is disabled, and record() returns on a single branch before
 * any formatting or allocation. Guard the call site with enabled() to also skip
 * building the row:
 * @code
 *   if (exporter.enabled()) exporter.record({tick, id, x, z});
 * @endcode
 *
 * Header-only and dependency-free, so it is unit-tested in isolation.
 */
namespace IKore {
namespace sim {

enum class ExportFormat { Csv, Json };

class DataExporter {
public:
    DataExporter() = default;

    /// Start streaming to @p out: writes the CSV header or opens the JSON array.
    void begin(std::ostream& out, ExportFormat format, std::vector<std::string> columns) {
        m_out = &out;
        m_format = format;
        m_columns = std::move(columns);
        m_rowCount = 0;
        m_ended = false;
        if (m_format == ExportFormat::Csv) {
            writeCsvHeader();
        } else {
            (*m_out) << "[\n";
        }
    }

    /// True while a stream is attached and end() has not been called.
    bool enabled() const { return m_out != nullptr && !m_ended; }
    /// Records written so far.
    std::size_t rowCount() const { return m_rowCount; }
    /// Round-trip precision for non-integral values (significant digits).
    void setPrecision(int significantDigits) {
        if (significantDigits > 0) m_precision = significantDigits;
    }

    /**
     * @brief Append one record. @p values should match the column count.
     *
     * No-op on a single branch if disabled - the hot path when export is off.
     */
    void record(const std::vector<double>& values) {
        if (!enabled()) return;
        if (m_format == ExportFormat::Csv) {
            writeCsvRow(values);
        } else {
            writeJsonRow(values);
        }
        ++m_rowCount;
    }

    /// Finish: close the JSON array and flush. Safe to call once; no-op if off.
    void end() {
        if (m_out == nullptr || m_ended) return;
        if (m_format == ExportFormat::Json) {
            if (m_rowCount > 0) (*m_out) << "\n";
            (*m_out) << "]\n";
        }
        m_out->flush();
        m_ended = true;
        m_out = nullptr;
    }

private:
    void appendNumber(std::string& out, double v) const {
        char buf[40];
        // Integral magnitudes print as plain integers (clean ids/ticks); others
        // use compact %g at the configured precision.
        if (std::isfinite(v) && v == std::floor(v) && std::fabs(v) < 9.0e15) {
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        } else {
            std::snprintf(buf, sizeof(buf), "%.*g", m_precision, v);
        }
        out += buf;
    }

    static void appendJsonString(std::string& out, const std::string& s) {
        out += '"';
        for (char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\t': out += "\\t"; break;
                case '\r': out += "\\r"; break;
                default: out += c; break;
            }
        }
        out += '"';
    }

    void writeCsvHeader() {
        std::string line;
        for (std::size_t i = 0; i < m_columns.size(); ++i) {
            if (i) line += ',';
            line += m_columns[i]; // column names are caller-controlled identifiers
        }
        line += '\n';
        (*m_out) << line;
    }

    void writeCsvRow(const std::vector<double>& values) {
        std::string line;
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (i) line += ',';
            appendNumber(line, values[i]);
        }
        line += '\n';
        (*m_out) << line;
    }

    void writeJsonRow(const std::vector<double>& values) {
        std::string line;
        if (m_rowCount > 0) line += ",\n";
        line += "  {";
        const std::size_t n = m_columns.size() < values.size() ? m_columns.size() : values.size();
        for (std::size_t i = 0; i < n; ++i) {
            if (i) line += ", ";
            appendJsonString(line, m_columns[i]);
            line += ':';
            appendNumber(line, values[i]);
        }
        line += '}';
        (*m_out) << line;
    }

    std::ostream* m_out{nullptr};
    ExportFormat m_format{ExportFormat::Csv};
    std::vector<std::string> m_columns;
    std::size_t m_rowCount{0};
    int m_precision{9};
    bool m_ended{false};
};

} // namespace sim
} // namespace IKore
