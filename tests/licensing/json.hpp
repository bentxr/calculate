#pragma once

#include <QByteArray>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The few JSON and SHA-256 calls the licensing checks make, over Qt's JSON and hashes.
namespace calculate::support {

class Json {
public:
    Json() = default;  // null

    static Json from(const QJsonValue& value) {
        Json json;
        if (value.isBool()) json.type_ = Type::Bool;
        if (value.isDouble()) json.type_ = Type::Number, json.number_ = value.toDouble();
        if (value.isString()) json.type_ = Type::String, json.text_ = value.toString().toStdString();
        if (value.isArray()) {
            json.type_ = Type::Array;
            for (const QJsonValue& item : value.toArray()) json.items_.push_back(from(item));
        }
        if (value.isObject()) {
            json.type_ = Type::Object;
            const QJsonObject object = value.toObject();
            for (auto it = object.begin(); it != object.end(); ++it)
                json.members_.emplace_back(it.key().toStdString(), from(it.value()));
        }
        return json;
    }

    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }
    double asNumber() const { return number_; }
    const std::string& asString() const { return text_; }
    std::size_t size() const { return items_.size(); }
    const std::vector<Json>& items() const { return items_; }
    const Json* find(std::string_view key) const {  // nullptr if absent or not an object
        for (const auto& member : members_)
            if (member.first == key) return &member.second;
        return nullptr;
    }

private:
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type_ = Type::Null;
    double number_ = 0;
    std::string text_;
    std::vector<Json> items_;
    std::vector<std::pair<std::string, Json>> members_;
};

struct JsonError {
    std::size_t offset = 0;  // byte offset of the problem
    std::string message;
};

inline std::optional<Json> parseJson(std::string_view text, JsonError* error) {
    QJsonParseError problem;
    const QJsonDocument document =
        QJsonDocument::fromJson(QByteArray(text.data(), static_cast<qsizetype>(text.size())), &problem);
    if (problem.error != QJsonParseError::NoError) {
        if (error) error->offset = static_cast<std::size_t>(problem.offset), error->message = problem.errorString().toStdString();
        return std::nullopt;
    }
    return Json::from(document.isArray() ? QJsonValue(document.array()) : QJsonValue(document.object()));
}

inline std::string sha256Hex(std::string_view bytes) {
    return QCryptographicHash::hash(QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())),
                                    QCryptographicHash::Sha256)
        .toHex()
        .toStdString();
}

}  // namespace calculate::support
