#include "vhid/device_profile.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <iomanip>
#include <sstream>

namespace vhid {

namespace {

constexpr int kProfileVersion = 1;

std::string bytes_to_hex(const std::vector<std::uint8_t>& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : data) {
        oss << std::setw(2) << static_cast<unsigned>(b);
    }
    return oss.str();
}

std::vector<std::uint8_t> hex_to_bytes(const std::string& hex) {
    std::vector<std::uint8_t> out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned v = 0;
        std::istringstream iss(hex.substr(i, 2));
        iss >> std::hex >> v;
        out.push_back(static_cast<std::uint8_t>(v));
    }
    return out;
}

nlohmann::json field_to_json(const FieldCaps& f) {
    return {
        {"usage_page",    f.usage_page},
        {"usage_min",     f.usage_min},
        {"usage_max",     f.usage_max},
        {"logical_min",   f.logical_min},
        {"logical_max",   f.logical_max},
        {"physical_min",  f.physical_min},
        {"physical_max",  f.physical_max},
        {"report_size",   f.report_size},
        {"report_count",  f.report_count},
        {"unit",          f.unit},
        {"unit_exponent", f.unit_exponent},
        {"is_button",     f.is_button},
        {"is_absolute",   f.is_absolute},
        {"has_null",      f.has_null},
    };
}

FieldCaps field_from_json(const nlohmann::json& j) {
    FieldCaps f;
    f.usage_page    = j.value("usage_page",    std::uint16_t{0});
    f.usage_min     = j.value("usage_min",     std::uint16_t{0});
    f.usage_max     = j.value("usage_max",     std::uint16_t{0});
    f.logical_min   = j.value("logical_min",   std::int32_t{0});
    f.logical_max   = j.value("logical_max",   std::int32_t{0});
    f.physical_min  = j.value("physical_min",  std::int32_t{0});
    f.physical_max  = j.value("physical_max",  std::int32_t{0});
    f.report_size   = j.value("report_size",   std::uint16_t{0});
    f.report_count  = j.value("report_count",  std::uint16_t{0});
    f.unit          = j.value("unit",          std::uint32_t{0});
    f.unit_exponent = j.value("unit_exponent", std::uint32_t{0});
    f.is_button     = j.value("is_button",     false);
    f.is_absolute   = j.value("is_absolute",   true);
    f.has_null      = j.value("has_null",      false);
    return f;
}

nlohmann::json report_to_json(const ReportInfo& r) {
    nlohmann::json fields = nlohmann::json::array();
    for (const auto& f : r.fields) fields.push_back(field_to_json(f));
    return {
        {"report_id",   r.report_id},
        {"byte_length", r.byte_length},
        {"fields",      fields},
    };
}

ReportInfo report_from_json(const nlohmann::json& j) {
    ReportInfo r;
    r.report_id   = j.value("report_id",   std::uint8_t{0});
    r.byte_length = j.value("byte_length", std::uint16_t{0});
    if (j.contains("fields") && j["fields"].is_array()) {
        for (const auto& f : j["fields"]) r.fields.push_back(field_from_json(f));
    }
    return r;
}

nlohmann::json reports_array(const std::vector<ReportInfo>& reports) {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& r : reports) arr.push_back(report_to_json(r));
    return arr;
}

std::vector<ReportInfo> reports_from_json(const nlohmann::json& arr) {
    std::vector<ReportInfo> out;
    if (arr.is_array()) {
        for (const auto& j : arr) out.push_back(report_from_json(j));
    }
    return out;
}

} // namespace

bool DeviceProfile::save(const std::string& path) const {
    nlohmann::json j;
    j["vhid_profile_version"] = kProfileVersion;
    j["vendor_id"]            = vendor_id;
    j["product_id"]           = product_id;
    j["version_number"]       = version_number;
    j["manufacturer"]         = manufacturer;
    j["product"]              = product;
    j["serial_number"]        = serial_number;
    j["usage_page"]           = usage_page;
    j["usage"]                = usage;
    j["input_reports"]        = reports_array(input_reports);
    j["output_reports"]       = reports_array(output_reports);
    j["feature_reports"]      = reports_array(feature_reports);
    j["report_descriptor"]    = bytes_to_hex(report_descriptor);

    std::ofstream ofs(path);
    if (!ofs) return false;
    ofs << j.dump(2);
    return ofs.good();
}

std::optional<DeviceProfile> DeviceProfile::load(const std::string& path) {
    std::ifstream ifs(path);
    if (!ifs) return std::nullopt;

    nlohmann::json j;
    try {
        ifs >> j;
    } catch (const std::exception&) {
        return std::nullopt;
    }

    DeviceProfile p;
    p.vendor_id       = j.value("vendor_id",      std::uint16_t{0});
    p.product_id      = j.value("product_id",     std::uint16_t{0});
    p.version_number  = j.value("version_number", std::uint16_t{0});
    p.manufacturer    = j.value("manufacturer",   std::string{});
    p.product         = j.value("product",        std::string{});
    p.serial_number   = j.value("serial_number",  std::string{});
    p.usage_page      = j.value("usage_page",     std::uint16_t{0});
    p.usage           = j.value("usage",          std::uint16_t{0});
    if (j.contains("input_reports"))   p.input_reports   = reports_from_json(j["input_reports"]);
    if (j.contains("output_reports"))  p.output_reports  = reports_from_json(j["output_reports"]);
    if (j.contains("feature_reports")) p.feature_reports = reports_from_json(j["feature_reports"]);
    if (j.contains("report_descriptor") && j["report_descriptor"].is_string()) {
        p.report_descriptor = hex_to_bytes(j["report_descriptor"].get<std::string>());
    }
    return p;
}

} // namespace vhid
