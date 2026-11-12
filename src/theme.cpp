#include "dreamui/theme.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dreamui {
namespace {

struct Json {
    enum class Type { Object, String, Number, Boolean, Null } type = Type::Null;
    std::map<std::string, Json> members;
    std::string string;
    std::uint64_t number = 0;
};

class Parser {
public:
    explicit Parser(std::string_view source) : source_(source) {}
    Json parse() {
        if (source_.size() > 512 * 1024) throw std::runtime_error("JSON exceeds 512 KiB limit");
        auto result = value(0);
        space();
        if (position_ != source_.size()) throw std::runtime_error("trailing JSON data");
        return result;
    }
private:
    std::string_view source_;
    std::size_t position_ = 0;
    void space() {
        while (position_ < source_.size() &&
               (source_[position_] == ' ' || source_[position_] == '\t' ||
                source_[position_] == '\n' || source_[position_] == '\r')) ++position_;
    }
    bool eat(char expected) {
        space();
        if (position_ < source_.size() && source_[position_] == expected) {
            ++position_;
            return true;
        }
        return false;
    }
    void require(char expected) {
        if (!eat(expected)) throw std::runtime_error("malformed JSON delimiter");
    }
    int hex() {
        if (position_ >= source_.size()) throw std::runtime_error("truncated Unicode escape");
        const char c = source_[position_++];
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        throw std::runtime_error("invalid Unicode escape");
    }
    void unicode(std::string& output) {
        std::uint32_t cp = 0;
        for (int i=0; i<4; ++i) cp = (cp << 4) | hex();
        if (cp >= 0xD800 && cp <= 0xDFFF)
            throw std::runtime_error("surrogate escapes unsupported: use UTF-8");
        if (cp < 0x80) output.push_back(static_cast<char>(cp));
        else if (cp < 0x800) {
            output.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            output.push_back(static_cast<char>(0x80 | (cp & 63)));
        } else {
            output.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            output.push_back(static_cast<char>(0x80 | ((cp >> 6) & 63)));
            output.push_back(static_cast<char>(0x80 | (cp & 63)));
        }
    }
    std::string string() {
        require('"');
        std::string result;
        while (position_ < source_.size()) {
            char c = source_[position_++];
            if (c == '"') return result;
            if (static_cast<unsigned char>(c) < 0x20) throw std::runtime_error("unescaped control character");
            if (c != '\\') {
                result.push_back(c);
            } else {
                if (position_ >= source_.size()) throw std::runtime_error("truncated JSON escape");
                c = source_[position_++];
                switch (c) {
                    case '"': case '\\': case '/': result.push_back(c); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'u': unicode(result); break;
                    default: throw std::runtime_error("invalid JSON escape");
                }
            }
            if (result.size() > 512 * 1024) throw std::runtime_error("JSON string too long");
        }
        throw std::runtime_error("unterminated JSON string");
    }
    Json value(int depth) {
        if (depth > 16) throw std::runtime_error("JSON nesting too deep");
        space();
        if (position_ >= source_.size()) throw std::runtime_error("unexpected end of JSON");
        const char c = source_[position_];
        if (c == '{') {
            ++position_;
            Json result;
            result.type = Json::Type::Object;
            if (eat('}')) return result;
            do {
                space();
                if (position_ >= source_.size() || source_[position_] != '"')
                    throw std::runtime_error("expected JSON object key");
                auto name = string();
                require(':');
                if (!result.members.emplace(std::move(name), value(depth + 1)).second)
                    throw std::runtime_error("duplicate JSON key");
                if (result.members.size() > 1024) throw std::runtime_error("too many JSON keys");
                if (eat('}')) return result;
                require(',');
            } while (true);
        }
        if (c == '"') {
            Json result;
            result.type = Json::Type::String;
            result.string = string();
            return result;
        }
        if (c >= '0' && c <= '9') {
            Json result;
            result.type = Json::Type::Number;
            do {
                std::uint64_t digit = static_cast<std::uint64_t>(source_[position_++] - '0');
                if (result.number > (std::numeric_limits<std::uint64_t>::max() - digit) / 10)
                    throw std::runtime_error("integer overflow");
                result.number = result.number * 10 + digit;
            } while (position_ < source_.size() && source_[position_] >= '0' && source_[position_] <= '9');
            return result;
        }
        for (auto literal : {"true", "false", "null"}) {
            if (source_.substr(position_, std::char_traits<char>::length(literal)) == literal) {
                position_ += std::char_traits<char>::length(literal);
                Json result;
                result.type = literal[0] == 'n' ? Json::Type::Null : Json::Type::Boolean;
                return result;
            }
        }
        throw std::runtime_error("unsupported JSON value");
    }
};

const Json& required(const Json& obj, std::string_view field, Json::Type kind) {
    if (obj.type != Json::Type::Object) throw std::runtime_error("expected JSON object");
    const auto it = obj.members.find(std::string(field));
    if (it == obj.members.end() || it->second.type != kind)
        throw std::runtime_error("missing or wrong type: " + std::string(field));
    return it->second;
}
bool valid_id(std::string_view id) {
    if (id.empty() || id.size() > 128) return false;
    bool previousSeparator = true;
    for (char c : id) {
        const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        if (!alnum && c != '.' && c != '-') return false;
        if (!alnum && previousSeparator) return false;
        previousSeparator = !alnum;
    }
    return !previousSeparator;
}
bool valid_semver(std::string_view version) {
    int groups = 0;
    std::size_t start = 0;
    while (true) {
        const auto dot = version.find('.', start);
        const auto piece = version.substr(start, dot == version.npos ? version.npos : dot - start);
        if (piece.empty() || (piece.size() > 1 && piece[0] == '0') || piece.size() > 10) return false;
        for (char c : piece) if (c < '0' || c > '9') return false;
        ++groups;
        if (dot == version.npos) return groups == 3;
        start = dot + 1;
        if (groups >= 3) return false;
    }
}
int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::runtime_error("invalid hex RGB color");
}
Rgb rgb(const std::string& color) {
    if (color.size() != 7 || color[0] != '#')
        throw std::runtime_error("color must use #RRGGBB");
    return {static_cast<std::uint8_t>((nibble(color[1]) << 4) | nibble(color[2])),
            static_cast<std::uint8_t>((nibble(color[3]) << 4) | nibble(color[4])),
            static_cast<std::uint8_t>((nibble(color[5]) << 4) | nibble(color[6]))};
}
void flatten(const Json& node, std::string prefix, std::map<std::string,Rgb>& result, int depth) {
    if (depth > 8) throw std::runtime_error("too many nested color groups");
    if (node.type == Json::Type::String) {
        if (prefix.empty()) throw std::runtime_error("empty color token");
        if (result.size() >= 256) throw std::runtime_error("too many color tokens");
        result.emplace(std::move(prefix), rgb(node.string));
    } else if (node.type == Json::Type::Object) {
        if (node.members.empty()) throw std::runtime_error("empty color group");
        for (const auto& [name, entry] : node.members) {
            if (name.empty() || name.size() > 64 || name.find('.') != name.npos)
                throw std::runtime_error("invalid color token name");
            flatten(entry, prefix.empty() ? name : prefix + "." + name, result, depth + 1);
        }
    } else {
        throw std::runtime_error("color groups must contain objects and #RRGGBB values");
    }
}
std::string bytes_to_string(const PackageEntry* entry, std::string_view label) {
    if (!entry) throw std::runtime_error("missing required member: " + std::string(label));
    if (entry->bytes.size() > 512 * 1024) throw std::runtime_error("JSON member too large");
    return {reinterpret_cast<const char*>(entry->bytes.data()), entry->bytes.size()};
}
} // namespace

ThemeResult parse_theme(const PackageResult& archive) {
    if (!archive) return {{}, archive.error};
    try {
        auto manifest = Parser(bytes_to_string(archive.find("manifest.json"), "manifest.json")).parse();
        auto colors = Parser(bytes_to_string(archive.find("theme/colors.json"), "theme/colors.json")).parse();
        if (required(manifest,"schemaVersion",Json::Type::Number).number != 1)
            throw std::runtime_error("unsupported .dream schemaVersion");
        if (required(manifest,"type",Json::Type::String).string != "theme")
            throw std::runtime_error("unsupported .dream type");
        Theme result;
        result.id = required(manifest,"id",Json::Type::String).string;
        result.name = required(manifest,"name",Json::Type::String).string;
        result.version = required(manifest,"version",Json::Type::String).string;
        if (!valid_id(result.id)) throw std::runtime_error("invalid theme id");
        if (result.name.empty() || result.name.size() > 128) throw std::runtime_error("invalid theme name");
        if (!valid_semver(result.version)) throw std::runtime_error("invalid theme version");
        flatten(colors,"",result.colors,0);
        if (result.colors.empty()) throw std::runtime_error("theme has no color tokens");
        return {std::move(result), ""};
    } catch (const std::exception& error) {
        return {{}, error.what()};
    }
}

const std::vector<InkBinding>& candidate_ink_bindings() {
    // Names are vanilla Ink style resource/property names. They must be
    // independently checked at runtime before an engine-backed writer is enabled.
    static const std::vector<InkBinding> bindings = {
        {"text.primary","base\\gameplay\\gui\\common\\main_colors.inkstyle","MainColors.PanelWhite"},
        {"text.secondary","base\\gameplay\\gui\\common\\main_colors.inkstyle","MainColors.Grey"},
        {"text.disabled","base\\gameplay\\gui\\common\\main_colors.inkstyle","MainColors.DarkGrey"},
        {"selection.active","base\\gameplay\\gui\\common\\main_colors.inkstyle","MainColors.ActiveYellow"}
    };
    return bindings;
}

bool InkColorSession::apply(const Theme& theme, InkColorWriter& writer, std::string& error) {
    if (active()) { error = "session already active"; return false; }
    writer_ = &writer;
    for (const auto& binding : candidate_ink_bindings()) {
        const auto it = theme.colors.find(std::string(binding.token));
        if (it == theme.colors.end()) continue;
        Rgb original{};
        if (!writer.read(binding.resource, binding.property, original)) {
            error = "failed to read vanilla Ink value: " + std::string(binding.property);
            restore();
            return false;
        }
        // Save before attempting the write: a failed backend write may still
        // have partially changed the resource, and must be rolled back.
        originals_.push_back({binding, original});
        if (!writer.write(binding.resource, binding.property, it->second)) {
            error = "failed to write Ink value: " + std::string(binding.property);
            restore();
            return false;
        }
    }
    writer_ = originals_.empty() ? nullptr : &writer;
    return true;
}
void InkColorSession::restore() noexcept {
    if (writer_) {
        for (auto it=originals_.rbegin(); it!=originals_.rend(); ++it)
            writer_->write(it->binding.resource,it->binding.property,it->color);
    }
    originals_.clear();
    writer_ = nullptr;
}
} // namespace dreamui
