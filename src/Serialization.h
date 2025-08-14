#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <type_traits>
#include <typeinfo>
#include <glm/glm.hpp>
#include "core/Entity.h"
#include "core/Logger.h"

namespace IKore {

    /**
     * @brief JSON-like data structure for serialization
     */
    class SerializationData {
    public:
        enum class Type {
            NULL_VALUE,
            BOOLEAN,
            INTEGER,
            FLOAT,
            STRING,
            ARRAY,
            OBJECT
        };

        SerializationData() : m_type(Type::NULL_VALUE) {}
        SerializationData(bool value) : m_type(Type::BOOLEAN), m_boolValue(value) {}
        SerializationData(int64_t value) : m_type(Type::INTEGER), m_intValue(value) {}
        SerializationData(double value) : m_type(Type::FLOAT), m_floatValue(value) {}
        SerializationData(const std::string& value) : m_type(Type::STRING), m_stringValue(value) {}

        // Type checking
        bool isNull() const { return m_type == Type::NULL_VALUE; }
        bool isBool() const { return m_type == Type::BOOLEAN; }
        bool isInt() const { return m_type == Type::INTEGER; }
        bool isFloat() const { return m_type == Type::FLOAT; }
        bool isString() const { return m_type == Type::STRING; }
        bool isArray() const { return m_type == Type::ARRAY; }
        bool isObject() const { return m_type == Type::OBJECT; }

        // Value getters
        bool asBool() const { return m_type == Type::BOOLEAN ? m_boolValue : false; }
        int64_t asInt() const { return m_type == Type::INTEGER ? m_intValue : 0; }
        double asFloat() const { return m_type == Type::FLOAT ? m_floatValue : 0.0; }
        const std::string& asString() const { 
            static const std::string empty;
            return m_type == Type::STRING ? m_stringValue : empty; 
        }

        // Array operations
        void makeArray() { m_type = Type::ARRAY; m_arrayValue.clear(); }
        void push(const SerializationData& value) {
            if (m_type != Type::ARRAY) makeArray();
            m_arrayValue.push_back(value);
        }
        size_t size() const { return m_type == Type::ARRAY ? m_arrayValue.size() : 0; }
        const SerializationData& operator[](size_t index) const {
            static const SerializationData null_value;
            if (m_type != Type::ARRAY || index >= m_arrayValue.size()) return null_value;
            return m_arrayValue[index];
        }
        SerializationData& operator[](size_t index) {
            if (m_type != Type::ARRAY) makeArray();
            if (index >= m_arrayValue.size()) m_arrayValue.resize(index + 1);
            return m_arrayValue[index];
        }

        // Object operations
        void makeObject() { m_type = Type::OBJECT; m_objectValue.clear(); }
        void set(const std::string& key, const SerializationData& value) {
            if (m_type != Type::OBJECT) makeObject();
            m_objectValue[key] = value;
        }
        bool has(const std::string& key) const {
            return m_type == Type::OBJECT && m_objectValue.find(key) != m_objectValue.end();
        }
        const SerializationData& get(const std::string& key) const {
            static const SerializationData null_value;
            if (m_type != Type::OBJECT) return null_value;
            auto it = m_objectValue.find(key);
            return it != m_objectValue.end() ? it->second : null_value;
        }
        SerializationData& operator[](const std::string& key) {
            if (m_type != Type::OBJECT) makeObject();
            return m_objectValue[key];
        }

        // GLM vector serialization helpers
        void setVec3(const glm::vec3& vec) {
            makeArray();
            push(SerializationData(static_cast<double>(vec.x)));
            push(SerializationData(static_cast<double>(vec.y)));
            push(SerializationData(static_cast<double>(vec.z)));
        }

        glm::vec3 asVec3() const {
            if (m_type == Type::ARRAY && m_arrayValue.size() >= 3) {
                return glm::vec3(
                    static_cast<float>(m_arrayValue[0].asFloat()),
                    static_cast<float>(m_arrayValue[1].asFloat()),
                    static_cast<float>(m_arrayValue[2].asFloat())
                );
            }
            return glm::vec3(0.0f);
        }

        void setMat4(const glm::mat4& mat) {
            makeArray();
            for (int i = 0; i < 4; ++i) {
                SerializationData row;
                row.makeArray();
                for (int j = 0; j < 4; ++j) {
                    row.push(SerializationData(static_cast<double>(mat[i][j])));
                }
                push(row);
            }
        }

        glm::mat4 asMat4() const {
            glm::mat4 result(1.0f);
            if (m_type == Type::ARRAY && m_arrayValue.size() >= 4) {
                for (int i = 0; i < 4; ++i) {
                    const auto& row = m_arrayValue[i];
                    if (row.isArray() && row.size() >= 4) {
                        for (int j = 0; j < 4; ++j) {
                            result[i][j] = static_cast<float>(row[j].asFloat());
                        }
                    }
                }
            }
            return result;
        }

        // Iterator support for objects
        auto begin() const { return m_objectValue.begin(); }
        auto end() const { return m_objectValue.end(); }

    private:
        Type m_type;
        bool m_boolValue = false;
        int64_t m_intValue = 0;
        double m_floatValue = 0.0;
        std::string m_stringValue;
        std::vector<SerializationData> m_arrayValue;
        std::unordered_map<std::string, SerializationData> m_objectValue;
    };

    /**
     * @brief Interface for serializable objects
     */
    class ISerializable {
    public:
        virtual ~ISerializable() = default;
        
        /**
         * @brief Serialize object to data
         * @param data Output serialization data
         */
        virtual void serialize(SerializationData& data) const = 0;
        
        /**
         * @brief Deserialize object from data
         * @param data Input serialization data
         * @return True if deserialization was successful
         */
        virtual bool deserialize(const SerializationData& data) = 0;

        /**
         * @brief Get the type name for serialization
         * @return Type name string
         */
        virtual std::string getSerializationType() const = 0;
    };

    /**
     * @brief JSON format handler
     */
    class JsonFormat {
    public:
        /**
         * @brief Convert SerializationData to JSON string
         * @param data Data to convert
         * @param indent Current indentation level
         * @return JSON string
         */
        static std::string toString(const SerializationData& data, int indent = 0);

        /**
         * @brief Parse JSON string to SerializationData
         * @param json JSON string to parse
         * @return Parsed SerializationData
         */
        static SerializationData fromString(const std::string& json);

        /**
         * @brief Save SerializationData to JSON file
         * @param data Data to save
         * @param filename Output filename
         * @return True if successful
         */
        static bool saveToFile(const SerializationData& data, const std::string& filename);

        /**
         * @brief Load SerializationData from JSON file
         * @param filename Input filename
         * @return Loaded SerializationData (null if failed)
         */
        static SerializationData loadFromFile(const std::string& filename);

    private:
        static std::string escape(const std::string& str);
        static std::string unescape(const std::string& str);
        static std::string getIndent(int level);
        
        // Simple JSON parser helpers
        static SerializationData parseValue(const std::string& json, size_t& pos);
        static SerializationData parseObject(const std::string& json, size_t& pos);
        static SerializationData parseArray(const std::string& json, size_t& pos);
        static std::string parseString(const std::string& json, size_t& pos);
        static double parseNumber(const std::string& json, size_t& pos);
        static void skipWhitespace(const std::string& json, size_t& pos);
    };

    /**
     * @brief Binary format handler for optimized storage
     */
    class BinaryFormat {
    public:
        /**
         * @brief Convert SerializationData to binary data
         * @param data Data to convert
         * @param output Binary output stream
         * @return True if successful
         */
        static bool toBinary(const SerializationData& data, std::ostream& output);

        /**
         * @brief Parse binary data to SerializationData
         * @param input Binary input stream
         * @return Parsed SerializationData
         */
        static SerializationData fromBinary(std::istream& input);

        /**
         * @brief Save SerializationData to binary file
         * @param data Data to save
         * @param filename Output filename
         * @return True if successful
         */
        static bool saveToFile(const SerializationData& data, const std::string& filename);

        /**
         * @brief Load SerializationData from binary file
         * @param filename Input filename
         * @return Loaded SerializationData (null if failed)
         */
        static SerializationData loadFromFile(const std::string& filename);

    private:
        static void writeType(SerializationData::Type type, std::ostream& output);
        static SerializationData::Type readType(std::istream& input);
        static void writeString(const std::string& str, std::ostream& output);
        static std::string readString(std::istream& input);
    };

    /**
     * @brief Entity registry for type-safe entity creation during deserialization
     */
    class EntityRegistry {
    public:
        using EntityFactory = std::function<std::shared_ptr<Entity>()>;

        /**
         * @brief Register an entity type with its factory function
         * @tparam T Entity type
         * @param typeName Type name for serialization
         */
        template<typename T>
        static void registerEntityType(const std::string& typeName) {
            static_assert(std::is_base_of_v<Entity, T>, "T must inherit from Entity");
            static_assert(std::is_base_of_v<ISerializable, T>, "T must implement ISerializable");
            
            getInstance().m_factories[typeName] = []() -> std::shared_ptr<Entity> {
                return std::make_shared<T>();
            };
            
            LOG_INFO("Registered entity type: " + typeName);
        }

        /**
         * @brief Create entity by type name
         * @param typeName Type name
         * @return Created entity or nullptr if type not found
         */
        static std::shared_ptr<Entity> createEntity(const std::string& typeName);

        /**
         * @brief Get all registered type names
         * @return Vector of registered type names
         */
        static std::vector<std::string> getRegisteredTypes();

        /**
         * @brief Check if type is registered
         * @param typeName Type name to check
         * @return True if registered
         */
        static bool isTypeRegistered(const std::string& typeName);

    private:
        static EntityRegistry& getInstance();
        
        std::unordered_map<std::string, EntityFactory> m_factories;
    };

    /**
     * @brief Main serialization manager for entities and scenes
     */
    class SerializationManager {
    public:
        enum class Format {
            JSON,
            BINARY
        };

        /**
         * @brief Get singleton instance
         * @return Reference to SerializationManager instance
         */
        static SerializationManager& getInstance();

        /**
         * @brief Serialize a single entity
         * @param entity Entity to serialize
         * @return Serialization data
         */
        SerializationData serializeEntity(std::shared_ptr<Entity> entity) const;

        /**
         * @brief Deserialize a single entity
         * @param data Serialization data
         * @return Deserialized entity or nullptr if failed
         */
        std::shared_ptr<Entity> deserializeEntity(const SerializationData& data) const;

        /**
         * @brief Serialize all entities in the entity manager
         * @return Serialization data for the scene
         */
        SerializationData serializeScene() const;

        /**
         * @brief Deserialize a scene (replaces current entities)
         * @param data Scene serialization data
         * @return True if successful
         */
        bool deserializeScene(const SerializationData& data) const;

        /**
         * @brief Save scene to file
         * @param filename Output filename
         * @param format File format (JSON or Binary)
         * @return True if successful
         */
        bool saveScene(const std::string& filename, Format format = Format::JSON) const;

        /**
         * @brief Load scene from file
         * @param filename Input filename
         * @param format File format (JSON or Binary, auto-detected if not specified)
         * @return True if successful
         */
        bool loadScene(const std::string& filename, Format format = Format::JSON) const;

        /**
         * @brief Save single entity to file
         * @param entity Entity to save
         * @param filename Output filename
         * @param format File format
         * @return True if successful
         */
        bool saveEntity(std::shared_ptr<Entity> entity, const std::string& filename, 
                       Format format = Format::JSON) const;

        /**
         * @brief Load single entity from file
         * @param filename Input filename
         * @param format File format
         * @return Loaded entity or nullptr if failed
         */
        std::shared_ptr<Entity> loadEntity(const std::string& filename, 
                                          Format format = Format::JSON) const;

        /**
         * @brief Get scene statistics
         */
        struct SceneStats {
            size_t totalEntities = 0;
            size_t serializableEntities = 0;
            size_t nonSerializableEntities = 0;
            std::unordered_map<std::string, size_t> entityTypesCounts;
            size_t fileSizeBytes = 0;
        };

        /**
         * @brief Get statistics for the current scene
         * @return Scene statistics
         */
        SceneStats getSceneStats() const;

        /**
         * @brief Validate file format
         * @param filename File to check
         * @return Detected format or JSON as default
         */
        static Format detectFormat(const std::string& filename);

    private:
        SerializationManager() = default;
        ~SerializationManager() = default;

        // Non-copyable and non-movable
        SerializationManager(const SerializationManager&) = delete;
        SerializationManager& operator=(const SerializationManager&) = delete;
        SerializationManager(SerializationManager&&) = delete;
        SerializationManager& operator=(SerializationManager&&) = delete;
    };

} // namespace IKore
