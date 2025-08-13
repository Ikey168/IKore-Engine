#include "Serialization.h"
#include "Entity.h"
#include "EntityTypes.h"
#include <sstream>
#include <cctype>
#include <algorithm>

namespace IKore {

    // ============================================================================
    // JsonFormat Implementation
    // ============================================================================

    std::string JsonFormat::toString(const SerializationData& data, int indent) {
        std::stringstream ss;
        std::string indentStr = getIndent(indent);
        std::string nextIndentStr = getIndent(indent + 1);

        if (data.isNull()) {
            ss << "null";
        } else if (data.isBool()) {
            ss << (data.asBool() ? "true" : "false");
        } else if (data.isInt()) {
            ss << data.asInt();
        } else if (data.isFloat()) {
            ss << std::fixed << std::setprecision(6) << data.asFloat();
        } else if (data.isString()) {
            ss << "\"" << escape(data.asString()) << "\"";
        } else if (data.isArray()) {
            if (data.size() == 0) {
                ss << "[]";
            } else {
                ss << "[\n";
                for (size_t i = 0; i < data.size(); ++i) {
                    ss << nextIndentStr << toString(data[i], indent + 1);
                    if (i < data.size() - 1) {
                        ss << ",";
                    }
                    ss << "\n";
                }
                ss << indentStr << "]";
            }
        } else if (data.isObject()) {
            auto objectData = data;
            if (objectData.begin() == objectData.end()) {
                ss << "{}";
            } else {
                ss << "{\n";
                auto it = objectData.begin();
                while (it != objectData.end()) {
                    ss << nextIndentStr << "\"" << escape(it->first) << "\": " 
                       << toString(it->second, indent + 1);
                    ++it;
                    if (it != objectData.end()) {
                        ss << ",";
                    }
                    ss << "\n";
                }
                ss << indentStr << "}";
            }
        }

        return ss.str();
    }

    SerializationData JsonFormat::fromString(const std::string& json) {
        size_t pos = 0;
        skipWhitespace(json, pos);
        return parseValue(json, pos);
    }

    bool JsonFormat::saveToFile(const SerializationData& data, const std::string& filename) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for writing: " + filename);
            return false;
        }

        std::string jsonString = toString(data);
        file << jsonString;
        file.close();

        LOG_INFO("Saved serialization data to: " + filename + " (" + std::to_string(jsonString.size()) + " bytes)");
        return true;
    }

    SerializationData JsonFormat::loadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open file for reading: " + filename);
            return SerializationData();
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        std::string jsonString = buffer.str();
        LOG_INFO("Loaded serialization data from: " + filename + " (" + std::to_string(jsonString.size()) + " bytes)");
        
        return fromString(jsonString);
    }

    // Helper methods for JsonFormat
    std::string JsonFormat::escape(const std::string& str) {
        std::string result;
        result.reserve(str.size() * 2);
        
        for (char c : str) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (c >= 0 && c < 32) {
                        std::stringstream ss;
                        ss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                        result += ss.str();
                    } else {
                        result += c;
                    }
                    break;
            }
        }
        
        return result;
    }

    std::string JsonFormat::unescape(const std::string& str) {
        std::string result;
        result.reserve(str.size());
        
        for (size_t i = 0; i < str.size(); ++i) {
            if (str[i] == '\\' && i + 1 < str.size()) {
                switch (str[i + 1]) {
                    case '"': result += '"'; ++i; break;
                    case '\\': result += '\\'; ++i; break;
                    case 'b': result += '\b'; ++i; break;
                    case 'f': result += '\f'; ++i; break;
                    case 'n': result += '\n'; ++i; break;
                    case 'r': result += '\r'; ++i; break;
                    case 't': result += '\t'; ++i; break;
                    case 'u': 
                        if (i + 5 < str.size()) {
                            std::string hex = str.substr(i + 2, 4);
                            try {
                                int codepoint = std::stoi(hex, nullptr, 16);
                                result += static_cast<char>(codepoint);
                                i += 5;
                            } catch (...) {
                                result += str[i];
                            }
                        } else {
                            result += str[i];
                        }
                        break;
                    default:
                        result += str[i];
                        break;
                }
            } else {
                result += str[i];
            }
        }
        
        return result;
    }

    std::string JsonFormat::getIndent(int level) {
        return std::string(level * 2, ' ');
    }

    void JsonFormat::skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(json[pos])) {
            ++pos;
        }
    }

    SerializationData JsonFormat::parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        
        if (pos >= json.size()) {
            return SerializationData();
        }

        char c = json[pos];
        
        if (c == '{') {
            return parseObject(json, pos);
        } else if (c == '[') {
            return parseArray(json, pos);
        } else if (c == '"') {
            return SerializationData(parseString(json, pos));
        } else if (c == 't' || c == 'f') {
            // Boolean
            if (pos + 4 <= json.size() && json.substr(pos, 4) == "true") {
                pos += 4;
                return SerializationData(true);
            } else if (pos + 5 <= json.size() && json.substr(pos, 5) == "false") {
                pos += 5;
                return SerializationData(false);
            }
        } else if (c == 'n') {
            // Null
            if (pos + 4 <= json.size() && json.substr(pos, 4) == "null") {
                pos += 4;
                return SerializationData();
            }
        } else if (c == '-' || std::isdigit(c)) {
            return SerializationData(parseNumber(json, pos));
        }

        // Invalid character
        LOG_ERROR("JSON parse error: unexpected character '" + std::string(1, c) + "' at position " + std::to_string(pos));
        return SerializationData();
    }

    SerializationData JsonFormat::parseObject(const std::string& json, size_t& pos) {
        SerializationData result;
        result.makeObject();
        
        ++pos; // Skip '{'
        skipWhitespace(json, pos);
        
        if (pos < json.size() && json[pos] == '}') {
            ++pos;
            return result;
        }

        while (pos < json.size()) {
            skipWhitespace(json, pos);
            
            // Parse key
            if (pos >= json.size() || json[pos] != '"') {
                LOG_ERROR("JSON parse error: expected string key at position " + std::to_string(pos));
                return SerializationData();
            }
            
            std::string key = parseString(json, pos);
            skipWhitespace(json, pos);
            
            // Expect ':'
            if (pos >= json.size() || json[pos] != ':') {
                LOG_ERROR("JSON parse error: expected ':' at position " + std::to_string(pos));
                return SerializationData();
            }
            ++pos;
            
            // Parse value
            SerializationData value = parseValue(json, pos);
            result.set(key, value);
            
            skipWhitespace(json, pos);
            
            if (pos >= json.size()) {
                LOG_ERROR("JSON parse error: unexpected end of input");
                return SerializationData();
            }
            
            if (json[pos] == '}') {
                ++pos;
                break;
            } else if (json[pos] == ',') {
                ++pos;
            } else {
                LOG_ERROR("JSON parse error: expected ',' or '}' at position " + std::to_string(pos));
                return SerializationData();
            }
        }
        
        return result;
    }

    SerializationData JsonFormat::parseArray(const std::string& json, size_t& pos) {
        SerializationData result;
        result.makeArray();
        
        ++pos; // Skip '['
        skipWhitespace(json, pos);
        
        if (pos < json.size() && json[pos] == ']') {
            ++pos;
            return result;
        }

        while (pos < json.size()) {
            SerializationData value = parseValue(json, pos);
            result.push(value);
            
            skipWhitespace(json, pos);
            
            if (pos >= json.size()) {
                LOG_ERROR("JSON parse error: unexpected end of input");
                return SerializationData();
            }
            
            if (json[pos] == ']') {
                ++pos;
                break;
            } else if (json[pos] == ',') {
                ++pos;
                skipWhitespace(json, pos);
            } else {
                LOG_ERROR("JSON parse error: expected ',' or ']' at position " + std::to_string(pos));
                return SerializationData();
            }
        }
        
        return result;
    }

    std::string JsonFormat::parseString(const std::string& json, size_t& pos) {
        if (pos >= json.size() || json[pos] != '"') {
            LOG_ERROR("JSON parse error: expected '\"' at position " + std::to_string(pos));
            return "";
        }
        
        ++pos; // Skip opening quote
        size_t start = pos;
        
        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                ++pos; // Skip escape character
            }
            ++pos;
        }
        
        if (pos >= json.size()) {
            LOG_ERROR("JSON parse error: unterminated string");
            return "";
        }
        
        std::string result = json.substr(start, pos - start);
        ++pos; // Skip closing quote
        
        return unescape(result);
    }

    double JsonFormat::parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        
        if (pos < json.size() && json[pos] == '-') {
            ++pos;
        }
        
        if (pos >= json.size() || !std::isdigit(json[pos])) {
            LOG_ERROR("JSON parse error: invalid number at position " + std::to_string(start));
            return 0.0;
        }
        
        while (pos < json.size() && std::isdigit(json[pos])) {
            ++pos;
        }
        
        if (pos < json.size() && json[pos] == '.') {
            ++pos;
            while (pos < json.size() && std::isdigit(json[pos])) {
                ++pos;
            }
        }
        
        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            ++pos;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) {
                ++pos;
            }
            while (pos < json.size() && std::isdigit(json[pos])) {
                ++pos;
            }
        }
        
        std::string numberStr = json.substr(start, pos - start);
        try {
            return std::stod(numberStr);
        } catch (...) {
            LOG_ERROR("JSON parse error: invalid number format: " + numberStr);
            return 0.0;
        }
    }

    // ============================================================================
    // BinaryFormat Implementation
    // ============================================================================

    bool BinaryFormat::toBinary(const SerializationData& data, std::ostream& output) {
        if (data.isNull()) {
            writeType(SerializationData::Type::NULL_VALUE, output);
        } else if (data.isBool()) {
            writeType(SerializationData::Type::BOOLEAN, output);
            bool value = data.asBool();
            output.write(reinterpret_cast<const char*>(&value), sizeof(bool));
        } else if (data.isInt()) {
            writeType(SerializationData::Type::INTEGER, output);
            int64_t value = data.asInt();
            output.write(reinterpret_cast<const char*>(&value), sizeof(int64_t));
        } else if (data.isFloat()) {
            writeType(SerializationData::Type::FLOAT, output);
            double value = data.asFloat();
            output.write(reinterpret_cast<const char*>(&value), sizeof(double));
        } else if (data.isString()) {
            writeType(SerializationData::Type::STRING, output);
            writeString(data.asString(), output);
        } else if (data.isArray()) {
            writeType(SerializationData::Type::ARRAY, output);
            uint32_t size = static_cast<uint32_t>(data.size());
            output.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
            for (size_t i = 0; i < data.size(); ++i) {
                if (!toBinary(data[i], output)) {
                    return false;
                }
            }
        } else if (data.isObject()) {
            writeType(SerializationData::Type::OBJECT, output);
            auto objectData = data;
            uint32_t size = static_cast<uint32_t>(std::distance(objectData.begin(), objectData.end()));
            output.write(reinterpret_cast<const char*>(&size), sizeof(uint32_t));
            for (const auto& pair : objectData) {
                writeString(pair.first, output);
                if (!toBinary(pair.second, output)) {
                    return false;
                }
            }
        }
        
        return output.good();
    }

    SerializationData BinaryFormat::fromBinary(std::istream& input) {
        SerializationData::Type type = readType(input);
        SerializationData result;
        
        switch (type) {
            case SerializationData::Type::NULL_VALUE:
                // result is already null
                break;
                
            case SerializationData::Type::BOOLEAN: {
                bool value;
                input.read(reinterpret_cast<char*>(&value), sizeof(bool));
                result = SerializationData(value);
                break;
            }
            
            case SerializationData::Type::INTEGER: {
                int64_t value;
                input.read(reinterpret_cast<char*>(&value), sizeof(int64_t));
                result = SerializationData(value);
                break;
            }
            
            case SerializationData::Type::FLOAT: {
                double value;
                input.read(reinterpret_cast<char*>(&value), sizeof(double));
                result = SerializationData(value);
                break;
            }
            
            case SerializationData::Type::STRING: {
                std::string value = readString(input);
                result = SerializationData(value);
                break;
            }
            
            case SerializationData::Type::ARRAY: {
                uint32_t size;
                input.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
                result.makeArray();
                for (uint32_t i = 0; i < size; ++i) {
                    SerializationData element = fromBinary(input);
                    result.push(element);
                }
                break;
            }
            
            case SerializationData::Type::OBJECT: {
                uint32_t size;
                input.read(reinterpret_cast<char*>(&size), sizeof(uint32_t));
                result.makeObject();
                for (uint32_t i = 0; i < size; ++i) {
                    std::string key = readString(input);
                    SerializationData value = fromBinary(input);
                    result.set(key, value);
                }
                break;
            }
        }
        
        return result;
    }

    bool BinaryFormat::saveToFile(const SerializationData& data, const std::string& filename) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open binary file for writing: " + filename);
            return false;
        }

        bool success = toBinary(data, file);
        file.close();

        if (success) {
            LOG_INFO("Saved binary serialization data to: " + filename);
        } else {
            LOG_ERROR("Failed to write binary data to: " + filename);
        }

        return success;
    }

    SerializationData BinaryFormat::loadFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            LOG_ERROR("Failed to open binary file for reading: " + filename);
            return SerializationData();
        }

        SerializationData result = fromBinary(file);
        file.close();

        LOG_INFO("Loaded binary serialization data from: " + filename);
        return result;
    }

    void BinaryFormat::writeType(SerializationData::Type type, std::ostream& output) {
        uint8_t typeValue = static_cast<uint8_t>(type);
        output.write(reinterpret_cast<const char*>(&typeValue), sizeof(uint8_t));
    }

    SerializationData::Type BinaryFormat::readType(std::istream& input) {
        uint8_t typeValue;
        input.read(reinterpret_cast<char*>(&typeValue), sizeof(uint8_t));
        return static_cast<SerializationData::Type>(typeValue);
    }

    void BinaryFormat::writeString(const std::string& str, std::ostream& output) {
        uint32_t length = static_cast<uint32_t>(str.size());
        output.write(reinterpret_cast<const char*>(&length), sizeof(uint32_t));
        if (length > 0) {
            output.write(str.data(), length);
        }
    }

    std::string BinaryFormat::readString(std::istream& input) {
        uint32_t length;
        input.read(reinterpret_cast<char*>(&length), sizeof(uint32_t));
        
        if (length == 0) {
            return "";
        }
        
        std::string result(length, '\0');
        input.read(&result[0], length);
        return result;
    }

    // ============================================================================
    // EntityRegistry Implementation
    // ============================================================================

    EntityRegistry& EntityRegistry::getInstance() {
        static EntityRegistry instance;
        return instance;
    }

    std::shared_ptr<Entity> EntityRegistry::createEntity(const std::string& typeName) {
        auto& registry = getInstance();
        auto it = registry.m_factories.find(typeName);
        
        if (it != registry.m_factories.end()) {
            return it->second();
        }
        
        LOG_ERROR("Unknown entity type: " + typeName);
        return nullptr;
    }

    std::vector<std::string> EntityRegistry::getRegisteredTypes() {
        auto& registry = getInstance();
        std::vector<std::string> types;
        
        for (const auto& pair : registry.m_factories) {
            types.push_back(pair.first);
        }
        
        return types;
    }

    bool EntityRegistry::isTypeRegistered(const std::string& typeName) {
        auto& registry = getInstance();
        return registry.m_factories.find(typeName) != registry.m_factories.end();
    }

    // ============================================================================
    // SerializationManager Implementation
    // ============================================================================

    SerializationManager& SerializationManager::getInstance() {
        static SerializationManager instance;
        return instance;
    }

    SerializationData SerializationManager::serializeEntity(std::shared_ptr<Entity> entity) const {
        if (!entity) {
            LOG_ERROR("Cannot serialize null entity");
            return SerializationData();
        }

        SerializationData data;
        data.makeObject();

        // Store entity ID
        data["id"] = SerializationData(static_cast<int64_t>(entity->getID()));

        // Check if entity is serializable
        auto serializable = std::dynamic_pointer_cast<ISerializable>(entity);
        if (serializable) {
            data["type"] = SerializationData(serializable->getSerializationType());
            
            SerializationData entityData;
            serializable->serialize(entityData);
            data["data"] = entityData;
            
            LOG_INFO("Serialized entity ID: " + std::to_string(entity->getID()) + 
                     " Type: " + serializable->getSerializationType());
        } else {
            LOG_WARNING("Entity ID " + std::to_string(entity->getID()) + " is not serializable");
            data["type"] = SerializationData("NonSerializable");
            data["data"] = SerializationData(); // null data
        }

        return data;
    }

    std::shared_ptr<Entity> SerializationManager::deserializeEntity(const SerializationData& data) const {
        if (!data.isObject() || !data.has("type")) {
            LOG_ERROR("Invalid entity serialization data");
            return nullptr;
        }

        std::string typeName = data.get("type").asString();
        if (typeName == "NonSerializable") {
            LOG_WARNING("Skipping non-serializable entity");
            return nullptr;
        }

        // Create entity using registry
        auto entity = EntityRegistry::createEntity(typeName);
        if (!entity) {
            LOG_ERROR("Failed to create entity of type: " + typeName);
            return nullptr;
        }

        // Deserialize entity data
        auto serializable = std::dynamic_pointer_cast<ISerializable>(entity);
        if (serializable && data.has("data")) {
            if (!serializable->deserialize(data.get("data"))) {
                LOG_ERROR("Failed to deserialize entity data for type: " + typeName);
                return nullptr;
            }
        }

        LOG_INFO("Deserialized entity Type: " + typeName + " ID: " + std::to_string(entity->getID()));
        return entity;
    }

    SerializationData SerializationManager::serializeScene() const {
        EntityManager& entityManager = EntityManager::getInstance();
        auto entities = entityManager.getAllEntities();

        SerializationData sceneData;
        sceneData.makeObject();

        // Scene metadata
        SerializationData metadata;
        metadata.makeObject();
        metadata["version"] = SerializationData("1.0");
        metadata["timestamp"] = SerializationData(static_cast<int64_t>(std::time(nullptr)));
        metadata["entityCount"] = SerializationData(static_cast<int64_t>(entities.size()));
        sceneData["metadata"] = metadata;

        // Serialize all entities
        SerializationData entitiesArray;
        entitiesArray.makeArray();

        size_t serializedCount = 0;
        for (auto entity : entities) {
            SerializationData entityData = serializeEntity(entity);
            if (!entityData.isNull()) {
                entitiesArray.push(entityData);
                ++serializedCount;
            }
        }

        sceneData["entities"] = entitiesArray;

        LOG_INFO("Serialized scene: " + std::to_string(serializedCount) + "/" + 
                std::to_string(entities.size()) + " entities");

        return sceneData;
    }

    bool SerializationManager::deserializeScene(const SerializationData& data) const {
        if (!data.isObject() || !data.has("entities")) {
            LOG_ERROR("Invalid scene serialization data");
            return false;
        }

        EntityManager& entityManager = EntityManager::getInstance();

        // Clear existing entities
        entityManager.clear();

        const SerializationData& entitiesArray = data.get("entities");
        if (!entitiesArray.isArray()) {
            LOG_ERROR("Scene entities data is not an array");
            return false;
        }

        size_t loadedCount = 0;
        for (size_t i = 0; i < entitiesArray.size(); ++i) {
            auto entity = deserializeEntity(entitiesArray[i]);
            if (entity) {
                entityManager.addEntity(entity);
                ++loadedCount;
            }
        }

        // Initialize all loaded entities
        entityManager.initializeAll();

        LOG_INFO("Deserialized scene: " + std::to_string(loadedCount) + "/" + 
                std::to_string(entitiesArray.size()) + " entities loaded");

        return true;
    }

    bool SerializationManager::saveScene(const std::string& filename, Format format) const {
        SerializationData sceneData = serializeScene();
        
        if (format == Format::JSON) {
            return JsonFormat::saveToFile(sceneData, filename);
        } else {
            return BinaryFormat::saveToFile(sceneData, filename);
        }
    }

    bool SerializationManager::loadScene(const std::string& filename, Format format) const {
        SerializationData sceneData;
        
        if (format == Format::JSON) {
            sceneData = JsonFormat::loadFromFile(filename);
        } else {
            sceneData = BinaryFormat::loadFromFile(filename);
        }

        if (sceneData.isNull()) {
            LOG_ERROR("Failed to load scene data from: " + filename);
            return false;
        }

        return deserializeScene(sceneData);
    }

    bool SerializationManager::saveEntity(std::shared_ptr<Entity> entity, const std::string& filename, 
                                         Format format) const {
        SerializationData entityData = serializeEntity(entity);
        
        if (entityData.isNull()) {
            LOG_ERROR("Failed to serialize entity");
            return false;
        }

        if (format == Format::JSON) {
            return JsonFormat::saveToFile(entityData, filename);
        } else {
            return BinaryFormat::saveToFile(entityData, filename);
        }
    }

    std::shared_ptr<Entity> SerializationManager::loadEntity(const std::string& filename, 
                                                            Format format) const {
        SerializationData entityData;
        
        if (format == Format::JSON) {
            entityData = JsonFormat::loadFromFile(filename);
        } else {
            entityData = BinaryFormat::loadFromFile(filename);
        }

        if (entityData.isNull()) {
            LOG_ERROR("Failed to load entity data from: " + filename);
            return nullptr;
        }

        return deserializeEntity(entityData);
    }

    SerializationManager::SceneStats SerializationManager::getSceneStats() const {
        SceneStats stats;
        
        EntityManager& entityManager = EntityManager::getInstance();
        auto entities = entityManager.getAllEntities();
        
        stats.totalEntities = entities.size();
        
        for (auto entity : entities) {
            auto serializable = std::dynamic_pointer_cast<ISerializable>(entity);
            if (serializable) {
                ++stats.serializableEntities;
                std::string typeName = serializable->getSerializationType();
                stats.entityTypesCounts[typeName]++;
            } else {
                ++stats.nonSerializableEntities;
            }
        }
        
        return stats;
    }

    SerializationManager::Format SerializationManager::detectFormat(const std::string& filename) {
        // Simple format detection based on file extension
        if (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".json") {
            return Format::JSON;
        } else if (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".bin") {
            return Format::BINARY;
        }
        
        // Default to JSON
        return Format::JSON;
    }

} // namespace IKore
