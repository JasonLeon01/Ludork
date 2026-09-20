#pragma once
#include <Runtime/RuntimeSession.hpp>
#include <Runtime/RuntimeValue.hpp>
#include <string>
#include <unordered_map>
#include <variant>

class ComponentRuntimeCache {
public:
    enum class Kind {
        Types,
        FieldDefaults,
        FieldMap,
        InheritedDefaults
    };
    struct Types {
        std::unordered_map<std::string, int> values;
    };
    struct FieldDefaults {
        std::unordered_map<std::string, int> values;
    };
    struct FieldMap {
        std::unordered_map<std::string, std::string> values;
    };
    struct InheritedDefaults {
        std::unordered_map<std::string, std::unordered_map<std::string, int>>
            values;
    };
    struct Entry {
        std::variant<Types, FieldDefaults, FieldMap, InheritedDefaults>
            descriptor;
        int nextSlot = 1;
        bool initialized = false;
    };
    class Lease {
    public:
        Lease(Kind kind, const RuntimeValue& key);
        ~Lease();
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;
        template <typename T>
        T& descriptor() {
            return std::get<T>(entry_->descriptor);
        }
        bool initialized() const noexcept;
        void complete() noexcept;
        int store(const RuntimeValue& value);
        RuntimeValue value(int slot) const;

    private:
        ludork::runtime::RuntimeScope scope_;
        int stackBase_;
        int cacheIndex_ = 0;
        int keyIndex_ = 0;
        int userdataIndex_ = 0;
        Entry* entry_ = nullptr;
    };
    void clear(lua_State* state) const noexcept;
};
ComponentRuntimeCache& componentRuntimeCache();
