#pragma once
#include <array>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>

#include <lua.hpp>

namespace adh::script {

    inline std::function<void(const std::string&)>& default_error_handler() {
        static thread_local std::function<void(const std::string&)> handler = [](const std::string& message) {
            std::println(stderr, "{}", message);
        };
        return handler;
    }

    inline void set_error_handler(std::function<void(const std::string&)> handler) {
        default_error_handler() = std::move(handler);
    }

    template <typename T>
    using ToPureType = std::remove_cv_t<std::remove_pointer_t<std::remove_reference_t<T>>>;

    template <typename T>
    concept Class = std::is_class_v<std::remove_cv_t<T>>;

    template <typename R>
    concept OwningResult =
        !std::is_same_v<std::remove_cv_t<R>, std::string_view> && !std::is_same_v<std::remove_cv_t<R>, const char*> &&
        !std::is_same_v<std::remove_cv_t<R>, char*>;

    template <typename T>
    inline constexpr bool is_boxed_v = Class<T> && !std::is_same_v<std::remove_cv_t<T>, std::string> &&
                                       !std::is_same_v<std::remove_cv_t<T>, std::string_view>;

    struct BoxBase {
        void* object;
        bool owned;
    };

    template <typename T>
    struct Box : BoxBase {
        T* get() const noexcept {
            return static_cast<T*>(object);
        }
    };

    using LuaFn = std::function<int(lua_State*)>;
    using VarFn = std::function<void(lua_State*, void*)>;

    struct TransparentStringHash {
        using is_transparent = void;
        [[nodiscard]] std::size_t operator()(std::string_view text) const noexcept {
            return std::hash<std::string_view>{}(text);
        }
    };

    template <typename V>
    using StringMap = std::unordered_map<std::string, V, TransparentStringHash, std::equal_to<>>;

    struct TypeEntry {
        std::string type_name;
        std::string metatable_name;
        bool extensible{ false };
        StringMap<std::pair<VarFn, VarFn>> variables;
        StringMap<LuaFn> methods;
        StringMap<LuaFn> constructors;
        std::function<void(void*)> deleter;
    };

    struct Registry {
        std::unordered_map<std::type_index, std::unique_ptr<TypeEntry>> types;
        StringMap<LuaFn> global_functions;
        std::function<void(const std::string&)> error_handler{ [](const std::string& message) {
            if (auto& handler = default_error_handler()) {
                handler(message);
            }
        } };
    };

    inline Registry* registry_of(lua_State* state) {
        return *static_cast<Registry**>(lua_getextraspace(state));
    }

    inline void report_error(lua_State* state, const std::string& message) {
        if (auto& handler = registry_of(state)->error_handler) {
            handler(message);
        }
    }

    inline void report_error(lua_State* state, const char* message) {
        report_error(state, std::string{ message != nullptr ? message : "unknown error" });
    }

    inline std::string pop_error(lua_State* state) {
        std::size_t length{};
        const char* text{ lua_tolstring(state, -1, &length) };
        std::string message{ text != nullptr ? std::string{ text, length } : std::string{ "unknown error" } };
        lua_pop(state, 1);
        return message;
    }

    inline TypeEntry* find_entry(lua_State* state, std::type_index type) {
        Registry* registry{ registry_of(state) };
        const auto it{ registry->types.find(type) };
        return it != registry->types.end() ? it->second.get() : nullptr;
    }

    template <typename T>
    TypeEntry* find_entry(lua_State* state) {
        return find_entry(state, std::type_index(typeid(T)));
    }

    template <typename T>
    T get_type(lua_State* state, int index) {
        if constexpr (!std::is_same_v<T, std::remove_cv_t<T>>) {
            return get_type<std::remove_cv_t<T>>(state, index);
        } else if constexpr (std::is_same_v<T, const char*>) {
            return lua_tostring(state, index);
        } else if constexpr (std::is_same_v<T, char*>) {
            static_assert(false, "adh::script: bind C strings as 'const char*'; Lua-owned strings are immutable");
        } else if constexpr (std::is_same_v<T, std::string>) {
            std::size_t length{};
            const char* s = lua_tolstring(state, index, &length);
            return s != nullptr ? std::string{ s, length } : std::string{};
        } else if constexpr (std::is_same_v<T, std::string_view>) {
            std::size_t length{};
            const char* s = lua_tolstring(state, index, &length);
            return s != nullptr ? std::string_view{ s, length } : std::string_view{};
        } else if constexpr (std::is_pointer_v<T>) {
            static_assert(is_boxed_v<std::remove_pointer_t<T>>,
                          "adh::script: pointer parameters must point to a registered class; take scalars and strings "
                          "by value");
            return static_cast<Box<ToPureType<T>>*>(lua_touserdata(state, index))->get();
        } else if constexpr (std::is_reference_v<T>) {
            static_assert(is_boxed_v<std::remove_reference_t<T>>,
                          "adh::script: take scalar and string parameters by value or by const reference");
            return *static_cast<Box<ToPureType<T>>*>(lua_touserdata(state, index))->get();
        } else if constexpr (std::is_class_v<T>) {
            return *static_cast<Box<T>*>(lua_touserdata(state, index))->get();
        } else if constexpr (std::is_same_v<T, bool>) {
            return lua_toboolean(state, index) != 0;
        } else if constexpr (std::is_floating_point_v<T>) {
            return static_cast<T>(lua_tonumber(state, index));
        } else if constexpr (std::is_integral_v<T>) {
            int is_integer{};
            lua_Integer value{ lua_tointegerx(state, index, &is_integer) };
            if (is_integer == 0) {
                const lua_Number number{ lua_tonumber(state, index) };
                constexpr lua_Number limit{ -static_cast<lua_Number>(LUA_MININTEGER) };
                value = number >= -limit && number < limit ? static_cast<lua_Integer>(number) : 0;
            }
            return static_cast<T>(value);
        } else {
            static_assert(false, "adh::script: unsupported parameter type");
        }
    }

    template <typename T>
    bool is_valid_arg(lua_State* state, int index) {
        using Pure = ToPureType<T>;
        if constexpr (std::is_class_v<Pure> && !std::is_same_v<Pure, std::string> &&
                      !std::is_same_v<Pure, std::string_view>) {
            TypeEntry* entry{ find_entry<Pure>(state) };
            const auto* box{ entry != nullptr ? static_cast<const BoxBase*>(
                                                    luaL_testudata(state, index, entry->metatable_name.c_str()))
                                              : nullptr };
            return box != nullptr && (std::is_pointer_v<std::remove_cvref_t<T>> || box->object != nullptr);
        } else {
            return true;
        }
    }

    template <typename T>
    using ArgType =
        std::conditional_t<is_boxed_v<std::remove_cvref_t<T>> ||
                               (std::is_lvalue_reference_v<T> && !std::is_const_v<std::remove_reference_t<T>>),
                           T, std::remove_cvref_t<T>>;

    template <typename... Args>
    bool valid_args([[maybe_unused]] lua_State* state, [[maybe_unused]] int base) {
        return [&]<std::size_t... I>(std::index_sequence<I...>) {
            return (is_valid_arg<Args>(state, base + static_cast<int>(I)) && ...);
        }(std::index_sequence_for<Args...>{});
    }

    template <typename T>
    void push_value(lua_State* state, T value) {
        if constexpr (std::is_same_v<T, bool>) {
            lua_pushboolean(state, value);
        } else if constexpr (std::is_same_v<T, const char*> || std::is_same_v<T, char*>) {
            lua_pushstring(state, value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            lua_pushlstring(state, value.data(), value.size());
        } else if constexpr (std::is_same_v<T, std::string_view>) {
            lua_pushlstring(state, value.data(), value.size());
        } else if constexpr (std::is_floating_point_v<T>) {
            lua_pushnumber(state, value);
        } else if constexpr (std::is_integral_v<T>) {
            lua_pushinteger(state, static_cast<lua_Integer>(value));
        } else {
            static_assert(false, "adh::script: unsupported value type");
        }
    }

    template <typename T>
    Box<T>* new_boxed(lua_State* state, T* object, bool owned, int nuvalue = 1) {
        auto* box = static_cast<Box<T>*>(lua_newuserdatauv(state, sizeof(Box<T>), nuvalue));
        box->object = object;
        box->owned = owned;
        if (TypeEntry * entry{ find_entry<T>(state) }) {
            luaL_getmetatable(state, entry->metatable_name.c_str());
            lua_setmetatable(state, -2);
        }
        return box;
    }

    template <typename R, typename V>
    void push_return(lua_State* state, V&& value, int nuvalue = 1) {
        using Pure = ToPureType<R>;
        if constexpr (std::is_same_v<std::decay_t<R>, const char*> || std::is_same_v<std::decay_t<R>, char*>) {
            lua_pushstring(state, value);
        } else if constexpr (std::is_same_v<std::decay_t<R>, std::string>) {
            lua_pushlstring(state, value.data(), value.size());
        } else if constexpr (std::is_same_v<std::decay_t<R>, std::string_view>) {
            lua_pushlstring(state, value.data(), value.size());
        } else if constexpr (std::is_pointer_v<R>) {
            static_assert(std::is_class_v<Pure>,
                          "adh::script: only registered classes can be returned by pointer; return scalars by value");
            new_boxed<Pure>(state, const_cast<Pure*>(value), false, nuvalue);
        } else if constexpr (std::is_reference_v<R> && std::is_class_v<Pure>) {
            new_boxed<Pure>(state, const_cast<Pure*>(&value), false, nuvalue);
        } else if constexpr (std::is_class_v<Pure>) {
            if (find_entry<Pure>(state) == nullptr) {
                report_error(state,
                             "adh::script: returning an object of an unregistered type; call register_type<T>() first");
                lua_pushnil(state);
            } else {
                new_boxed<Pure>(state, new Pure(std::forward<V>(value)), true);
            }
        } else {
            push_value<std::decay_t<R>>(state, value);
        }
    }

    template <typename R, typename V>
    void push_result(lua_State* state, V&& value, int arguments) {
        if constexpr ((std::is_reference_v<R> || std::is_pointer_v<R>) && is_boxed_v<ToPureType<R>>) {
            const int top{ lua_gettop(state) };
            const int anchored{ arguments < top ? arguments : top };
            push_return<R>(state, std::forward<V>(value), 1 + anchored);
            for (int index{ 1 }; index <= anchored; ++index) {
                lua_pushvalue(state, index);
                lua_setiuservalue(state, -2, 1 + index);
            }
        } else {
            push_return<R>(state, std::forward<V>(value));
        }
    }

    template <Class T>
    void push_borrowed(lua_State* state, T* object) {
        new_boxed<T>(state, object, false);
    }

    template <typename T>
    void push_arg(lua_State* state, T&& value) {
        using Decayed = std::decay_t<T>;
        if constexpr (std::is_pointer_v<Decayed>) {
            using Pointee = std::remove_pointer_t<Decayed>;
            if constexpr (std::is_class_v<Pointee> && !std::is_same_v<Pointee, std::string> &&
                          !std::is_same_v<Pointee, std::string_view>) {
                push_borrowed<Pointee>(state, value);
            } else {
                push_value<Decayed>(state, value);
            }
        } else if constexpr (std::is_class_v<Decayed> && !std::is_same_v<Decayed, std::string> &&
                             !std::is_same_v<Decayed, std::string_view>) {
            push_borrowed<Decayed>(state, &value);
        } else {
            push_value<Decayed>(state, value);
        }
    }

    template <typename T, typename... Args, std::size_t... I>
    int construct(lua_State* state, TypeEntry* entry, std::index_sequence<I...> /*unused*/) {
        if (!valid_args<Args...>(state, 1)) {
            report_error(state, "adh::script: constructor for '" +
                                    (entry != nullptr ? entry->type_name : std::string{ "?" }) +
                                    "' called with an argument of the wrong type");
            return 0;
        }
        T* object = new T(get_type<ArgType<Args>>(state, 1 + static_cast<int>(I))...);
        new_boxed<T>(state, object, true);
        return 1;
    }

    template <typename T, typename R, typename... Args, typename Method, std::size_t... I>
    int call_method(lua_State* state, TypeEntry* entry, Method method, std::index_sequence<I...> /*unused*/) {
        Box<T>* box{ entry != nullptr ? static_cast<Box<T>*>(luaL_testudata(state, 1, entry->metatable_name.c_str()))
                                      : nullptr };
        if (box == nullptr || box->object == nullptr) {
            report_error(state, "adh::script: method called on a null or wrong-type object");
            return 0;
        }
        if (!valid_args<Args...>(state, 2)) {
            report_error(state,
                         "adh::script: method '" + entry->type_name + "' called with an argument of the wrong type");
            return 0;
        }
        if constexpr (std::is_void_v<R>) {
            (box->get()->*method)(get_type<ArgType<Args>>(state, 2 + static_cast<int>(I))...);
            return 0;
        } else {
            decltype(auto) result = (box->get()->*method)(get_type<ArgType<Args>>(state, 2 + static_cast<int>(I))...);
            push_result<R>(state, std::forward<decltype(result)>(result), 1 + static_cast<int>(sizeof...(Args)));
            return 1;
        }
    }

    inline BoxBase* entry_self(lua_State* state, const TypeEntry* entry) {
        return static_cast<BoxBase*>(luaL_testudata(state, 1, entry->metatable_name.c_str()));
    }

    inline int entry_index(lua_State* state) {
        auto* entry = static_cast<TypeEntry*>(lua_touserdata(state, lua_upvalueindex(1)));
        BoxBase* self{ entry_self(state, entry) };
        if (self == nullptr) {
            report_error(state, "adh::script: '" + entry->type_name + "' metamethod called on a value of another type");
            lua_pushnil(state);
            return 1;
        }
        void* object = self->object;
        if (lua_type(state, 2) == LUA_TSTRING) {
            const char* key = lua_tostring(state, 2);
            if (const auto it{ entry->variables.find(key) }; it != entry->variables.end()) {
                if (object == nullptr) {
                    report_error(state, "adh::script: member '" + std::string{ key } + "' read on a null object");
                    lua_pushnil(state);
                    return 1;
                }
                it->second.first(state, object);
                return 1;
            }
            if (entry->methods.contains(key)) {
                lua_pushvalue(state, lua_upvalueindex(2));
                lua_getfield(state, -1, key);
                lua_remove(state, -2);
                return 1;
            }
        }
        if (entry->extensible) {
            if (lua_getiuservalue(state, 1, 1) == LUA_TTABLE) {
                lua_pushvalue(state, 2);
                lua_gettable(state, -2);
                lua_remove(state, -2);
                return 1;
            }
            lua_pop(state, 1);
        }
        lua_pushnil(state);
        return 1;
    }

    inline int entry_new_index(lua_State* state) {
        auto* entry = static_cast<TypeEntry*>(lua_touserdata(state, lua_upvalueindex(1)));
        BoxBase* self{ entry_self(state, entry) };
        if (self == nullptr) {
            report_error(state, "adh::script: '" + entry->type_name + "' metamethod called on a value of another type");
            return 0;
        }
        void* object = self->object;
        if (lua_type(state, 2) == LUA_TSTRING) {
            const char* key = lua_tostring(state, 2);
            if (const auto it{ entry->variables.find(key) }; it != entry->variables.end()) {
                if (object == nullptr) {
                    report_error(state,
                                 "adh::script: cannot assign member '" + std::string{ key } + "' on a null object");
                    return 0;
                }
                it->second.second(state, object);
                return 0;
            }
        }
        if (entry->extensible) {
            if (lua_getiuservalue(state, 1, 1) != LUA_TTABLE) {
                lua_pop(state, 1);
                lua_newtable(state);
                lua_pushvalue(state, -1);
                lua_setiuservalue(state, 1, 1);
            }
            lua_pushvalue(state, 2);
            lua_pushvalue(state, 3);
            lua_settable(state, -3);
            lua_pop(state, 1);
            return 0;
        }
        const char* key = lua_tostring(state, 2);
        report_error(state, "adh::script: '" + std::string{ key != nullptr ? key : "?" } +
                                "' is not a member of type '" + entry->type_name + "' (type is not extensible)");
        return 0;
    }

    inline int entry_destructor(lua_State* state) {
        auto* entry = static_cast<TypeEntry*>(lua_touserdata(state, lua_upvalueindex(1)));
        BoxBase* box{ entry_self(state, entry) };
        if (box != nullptr && box->owned && box->object != nullptr && entry->deleter) {
            entry->deleter(box->object);
            box->object = nullptr;
        }
        return 0;
    }

    inline int entry_invoke(lua_State* state) {
        auto* fn = static_cast<LuaFn*>(lua_touserdata(state, lua_upvalueindex(1)));
        return (*fn)(state);
    }
    inline int entry_invoke_as_call(lua_State* state) {
        if (lua_gettop(state) == 0) {
            report_error(state, "adh::script: a type's __call metamethod was called without the type");
            return 0;
        }
        lua_remove(state, 1);
        auto* fn = static_cast<LuaFn*>(lua_touserdata(state, lua_upvalueindex(1)));
        return (*fn)(state);
    }

    template <lua_CFunction F>
    int guarded(lua_State* state) {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
        constexpr std::size_t MESSAGE_SIZE{ 256 };
        std::array<char, MESSAGE_SIZE> message;
        try {
            return F(state);
        } catch (const std::exception& error) {
            std::snprintf(message.data(), message.size(), "%s", error.what());
        }
        lua_pushstring(state, message.data());
        return lua_error(state);
#else
        return F(state);
#endif
    }

    template <typename T, typename V>
    void add_variable(TypeEntry& entry, const char* key, V T::* member) {
        if constexpr (std::is_class_v<std::decay_t<V>> && !std::is_same_v<std::decay_t<V>, std::string> &&
                      !std::is_same_v<std::decay_t<V>, std::string_view>) {
            using R = std::decay_t<V>;
            entry.variables[key].first = [member](lua_State* state, void* self) {
                T* object = static_cast<T*>(self);
                new_boxed<R>(state, &(object->*member), false, 2);
                lua_pushvalue(state, 1);
                lua_setiuservalue(state, -2, 2);
            };
            entry.variables[key].second = [member, name = std::string{ key }](lua_State* state, void* self) {
                T* object = static_cast<T*>(self);
                TypeEntry* type{ find_entry<R>(state) };
                Box<R>* box{ type != nullptr
                                 ? static_cast<Box<R>*>(luaL_testudata(state, 3, type->metatable_name.c_str()))
                                 : nullptr };
                if (box == nullptr || box->object == nullptr) {
                    report_error(state, "adh::script: cannot assign to member '" + name +
                                            "', expected a registered object of the matching type");
                    return;
                }
                object->*member = *(box->get());
            };
        } else {
            entry.variables[key].first = [member](lua_State* state, void* self) {
                push_value<std::decay_t<V>>(state, static_cast<T*>(self)->*member);
            };
            if constexpr (std::is_same_v<std::decay_t<V>, std::string_view> ||
                          std::is_same_v<std::decay_t<V>, const char*>) {
                entry.variables[key].second = [name = std::string{ key }](lua_State* state, void*) {
                    report_error(state, "adh::script: member '" + name +
                                            "' is a non-owning string (std::string_view or const char*) and cannot "
                                            "be assigned from Lua; use std::string");
                };
            } else {
                entry.variables[key].second = [member](lua_State* state, void* self) {
                    static_cast<T*>(self)->*member = get_type<std::decay_t<V>>(state, 3);
                };
            }
        }
    }

    inline int protected_get(lua_State* state) {
        lua_gettable(state, 1);
        return 1;
    }

    inline int protected_set(lua_State* state) {
        lua_settable(state, 1);
        return 0;
    }

    inline void set_global(lua_State* state, const char* name) {
        lua_pushglobaltable(state);
        lua_pushstring(state, name);
        lua_rotate(state, -3, -1);
        lua_rawset(state, -3);
        lua_pop(state, 1);
    }

    inline int get_global(lua_State* state, const char* name) {
        lua_pushglobaltable(state);
        lua_pushstring(state, name);
        const int type{ lua_rawget(state, -2) };
        lua_remove(state, -2);
        return type;
    }

    inline std::string chunk_key(const std::string& id) {
        return id + ":chunk";
    }

    inline void install_sandboxed_chunk(lua_State* state, const std::string& id) {
        const int chunk{ lua_gettop(state) };
        lua_newtable(state);
        lua_newtable(state);
        lua_pushglobaltable(state);
        lua_setfield(state, -2, "__index");
        lua_setmetatable(state, -2);
        lua_pushvalue(state, -1);
        lua_setfield(state, LUA_REGISTRYINDEX, id.c_str());
        lua_setupvalue(state, chunk, 1);
        lua_setfield(state, LUA_REGISTRYINDEX, chunk_key(id).c_str());
    }

    class Script {
        friend class State;

      public:
        Script() = default;

        [[nodiscard]] bool valid() const noexcept {
            return m_state != nullptr;
        }

        void bind() {
            if (m_state == nullptr) {
                return;
            }
            lua_getfield(m_state, LUA_REGISTRYINDEX, m_id.c_str());
        }
        void unbind() {
            if (m_state == nullptr) {
                return;
            }
            lua_pop(m_state, 1);
        }

        void run() {
            if (m_state == nullptr) {
                return;
            }
            lua_getfield(m_state, LUA_REGISTRYINDEX, chunk_key(m_id).c_str());
            if (lua_pcall(m_state, 0, 0, 0) != LUA_OK) {
                report_script_error(nullptr, pop_error(m_state).c_str());
            }
        }

        [[nodiscard]] bool reload() {
            if (m_state == nullptr) {
                return false;
            }
            const int status{ m_is_file ? luaL_loadfile(m_state, m_source.c_str())
                                        : luaL_loadstring(m_state, m_source.c_str()) };
            if (status != LUA_OK) {
                report_script_error(nullptr, pop_error(m_state).c_str());
                return false;
            }
            install_sandboxed_chunk(m_state, m_id);
            stamp_load_time();
            return true;
        }

        [[nodiscard]] bool needs_reload() const {
            if (!m_is_file) {
                return false;
            }
            std::error_code error;
            const auto current{ std::filesystem::last_write_time(m_source, error) };
            return !error && current != m_load_time;
        }

        template <typename R = void, typename... Args>
            requires OwningResult<R>
        R call(const char* function, Args&&... args) {
            const StackGuard guard{ m_state };
            bind();
            return call_bound<R>(function, std::forward<Args>(args)...);
        }

        template <typename R = void, typename... Args>
            requires OwningResult<R>
        R call_bound(const char* function, Args&&... args) {
            static_assert(std::is_void_v<R> || std::is_pointer_v<R> ||
                              (!std::is_reference_v<R> && std::is_default_constructible_v<R>),
                          "adh::script: call<R> must return void, a pointer, or a default-constructible "
                          "value; return registered objects by pointer");
            if (m_state == nullptr) {
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                } else {
                    return;
                }
            }
            get_field(function);
            if (lua_isfunction(m_state, -1) == 0) {
                lua_pop(m_state, 1);
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                } else {
                    return;
                }
            }
            if (lua_checkstack(m_state, static_cast<int>(sizeof...(Args)) + 1) == 0) {
                lua_pop(m_state, 1);
                report_script_error(function, "too many arguments for the Lua stack");
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                } else {
                    return;
                }
            }
            (push_arg(m_state, std::forward<Args>(args)), ...);
            constexpr int results{ std::is_void_v<R> ? 0 : 1 };
            if (lua_pcall(m_state, sizeof...(Args), results, 0) != LUA_OK) {
                report_script_error(function, pop_error(m_state).c_str());
                if constexpr (!std::is_void_v<R>) {
                    return R{};
                } else {
                    return;
                }
            }
            if constexpr (!std::is_void_v<R>) {
                if (!is_valid_arg<R>(m_state, -1)) {
                    lua_pop(m_state, 1);
                    report_script_error(function, "returned a value of an unexpected type");
                    return R{};
                }
                R value{ get_type<R>(m_state, -1) };
                lua_pop(m_state, 1);
                return value;
            }
        }

        [[nodiscard]] bool has_number(const char* name) {
            if (m_state == nullptr) {
                return false;
            }
            const StackGuard guard{ m_state };
            bind();
            get_field(name);
            return lua_isnumber(m_state, -1) != 0;
        }

        [[nodiscard]] double get_number(const char* name) {
            if (m_state == nullptr) {
                return 0.0;
            }
            const StackGuard guard{ m_state };
            bind();
            get_field(name);
            return lua_tonumber(m_state, -1);
        }

        void set_number(const char* name, double value) {
            set_field<double>(name, value);
        }

        [[nodiscard]] bool get_bool(const char* name) {
            if (m_state == nullptr) {
                return false;
            }
            const StackGuard guard{ m_state };
            bind();
            get_field(name);
            return lua_toboolean(m_state, -1) != 0;
        }

        void set_bool(const char* name, bool value) {
            set_field<bool>(name, value);
        }

        [[nodiscard]] std::string get_string(const char* name) {
            if (m_state == nullptr) {
                return std::string{};
            }
            const StackGuard guard{ m_state };
            bind();
            get_field(name);
            std::size_t length{};
            const char* text{ lua_tolstring(m_state, -1, &length) };
            return text != nullptr ? std::string{ text, length } : std::string{};
        }

        void set_string(const char* name, const char* value) {
            set_field<const char*>(name, value);
        }

        template <Class T>
        [[nodiscard]] T* get_object(const char* name) {
            if (m_state == nullptr) {
                return nullptr;
            }
            const StackGuard guard{ m_state };
            Box<T>* box{ lookup_box<T>(name) };
            return box != nullptr ? box->get() : nullptr;
        }

        template <Class T>
        [[nodiscard]] T* take_object(const char* name) {
            if (m_state == nullptr) {
                return nullptr;
            }
            const StackGuard guard{ m_state };
            Box<T>* box{ lookup_box<T>(name) };
            T* object{ nullptr };
            if (box != nullptr && box->owned) {
                object = box->get();
                box->owned = false;
            }
            return object;
        }

      private:
        void get_field(const char* name) {
            lua_pushcfunction(m_state, &protected_get);
            lua_pushvalue(m_state, -2);
            lua_pushstring(m_state, name);
            if (lua_pcall(m_state, 2, 1, 0) != LUA_OK) {
                report_script_error(name, pop_error(m_state).c_str());
                lua_pushnil(m_state);
            }
        }

        template <typename T>
        void set_field(const char* name, T value) {
            if (m_state == nullptr) {
                return;
            }
            lua_pushcfunction(m_state, &protected_set);
            bind();
            lua_pushstring(m_state, name);
            push_value<T>(m_state, value);
            if (lua_pcall(m_state, 3, 0, 0) != LUA_OK) {
                report_script_error(name, pop_error(m_state).c_str());
            }
        }

        template <Class T>
        Box<T>* lookup_box(const char* name) {
            bind();
            get_field(name);
            TypeEntry* entry{ find_entry<T>(m_state) };
            return entry != nullptr ? static_cast<Box<T>*>(luaL_testudata(m_state, -1, entry->metatable_name.c_str()))
                                    : nullptr;
        }

        Script(std::string id, lua_State* state, std::string source, bool is_file)
            : m_id{ std::move(id) }
            , m_state{ state }
            , m_source{ std::move(source) }
            , m_is_file{ is_file } {
            stamp_load_time();
        }

        void stamp_load_time() {
            if (m_is_file) {
                std::error_code error;
                m_load_time = std::filesystem::last_write_time(m_source, error);
            }
        }

        void report_script_error(const char* function, const char* message) {
            std::string out{ '[' + m_id };
            if (function != nullptr) {
                out += ':';
                out += function;
            }
            out += "] ";
            out += (message != nullptr ? message : "unknown error");
            report_error(m_state, out);
        }

        class StackGuard {
          public:
            explicit StackGuard(lua_State* state)
                : m_state{ state }
                , m_top{ state != nullptr ? lua_gettop(state) : 0 } {}
            StackGuard(const StackGuard&) = delete;
            StackGuard& operator=(const StackGuard&) = delete;
            ~StackGuard() {
                if (m_state != nullptr) {
                    lua_settop(m_state, m_top);
                }
            }

          private:
            lua_State* m_state;
            int m_top;
        };

        std::string m_id;
        lua_State* m_state{ nullptr };
        std::string m_source;
        bool m_is_file{ false };
        std::filesystem::file_time_type m_load_time;
    };

    template <typename T>
    struct CallableSignature;
    template <typename R, typename... Args>
    struct CallableSignature<R (*)(Args...)> {
        using Pointer = R (*)(Args...);
    };
    template <typename R, typename... Args>
    struct CallableSignature<R (&)(Args...)> {
        using Pointer = R (*)(Args...);
    };
    template <typename R, typename... Args>
    struct CallableSignature<R(Args...)> {
        using Pointer = R (*)(Args...);
    };
    template <typename R, typename... Args>
    struct CallableSignature<R (*)(Args...) noexcept> {
        using Pointer = R (*)(Args...);
    };
    template <typename R, typename... Args>
    struct CallableSignature<R (&)(Args...) noexcept> {
        using Pointer = R (*)(Args...);
    };
    template <typename R, typename... Args>
    struct CallableSignature<R(Args...) noexcept> {
        using Pointer = R (*)(Args...);
    };
    template <typename C, typename R, typename... Args>
    struct CallableSignature<R (C::*)(Args...) const> {
        using Pointer = R (*)(Args...);
    };
    template <typename C, typename R, typename... Args>
    struct CallableSignature<R (C::*)(Args...) const noexcept> {
        using Pointer = R (*)(Args...);
    };
    template <typename C, typename R, typename... Args>
    struct CallableSignature<R (C::*)(Args...)> {
        using Pointer = R (*)(Args...);
    };
    template <typename C, typename R, typename... Args>
    struct CallableSignature<R (C::*)(Args...) noexcept> {
        using Pointer = R (*)(Args...);
    };
    template <typename F>
    struct CallableSignature : CallableSignature<decltype(&F::operator())> {};

    class State {
      public:
        State() = default;
        explicit State(lua_State* state)
            : m_state{ state }
            , m_registry{ state != nullptr ? std::make_unique<Registry>() : nullptr } {
            if (m_state != nullptr) {
                *static_cast<Registry**>(lua_getextraspace(m_state)) = m_registry.get();
            }
        }
        State(const State&) = delete;
        State& operator=(const State&) = delete;
        State(State&& other) noexcept
            : m_state{ other.m_state }
            , m_registry{ std::move(other.m_registry) }
            , m_global_names{ std::move(other.m_global_names) }
            , m_next_id{ other.m_next_id } {
            other.m_state = nullptr;
        }
        State& operator=(State&& other) noexcept {
            if (this != &other) {
                close();
                m_state = other.m_state;
                m_registry = std::move(other.m_registry);
                m_global_names = std::move(other.m_global_names);
                m_next_id = other.m_next_id;
                other.m_state = nullptr;
            }
            return *this;
        }
        ~State() {
            close();
        }

        [[nodiscard]] lua_State* handle() const noexcept {
            return m_state;
        }
        explicit operator bool() const noexcept {
            return m_state != nullptr;
        }

        void set_error_handler(std::function<void(const std::string&)> handler) {
            if (m_registry) {
                m_registry->error_handler = std::move(handler);
            }
        }

        template <typename F>
            requires std::is_copy_constructible_v<F>
        void register_function(const char* name, F function) {
            const std::uint32_t have = role_of(name);
            if ((have & (FUNCTION | EXCLUSIVE)) != 0U) {
                report_error(m_state,
                             std::string{ "adh::script: global '" } + name + "' is already registered as a function");
                return;
            }
            LuaFn* slot = store_callable(name, std::move(function), typename CallableSignature<F>::Pointer{});
            m_global_names[name] |= FUNCTION;
            if ((have & TYPE) != 0U) {
                make_type_callable(name, slot);
            } else {
                lua_pushlightuserdata(m_state, slot);
                lua_pushcclosure(m_state, &guarded<&entry_invoke>, 1);
                set_global(m_state, name);
            }
        }

        template <Class T>
        void register_type(const std::string& name, bool extensible = false) {
            const std::uint32_t have = role_of(name.c_str());
            if ((have & (TYPE | EXCLUSIVE)) != 0U) {
                report_error(m_state, "adh::script: global '" + name + "' is already registered as a type");
                return;
            }
            TypeEntry& e = entry<T>();
            e.type_name = name;
            e.metatable_name = name + "_meta";
            e.extensible = extensible;
            e.deleter = [](void* object) {
                delete static_cast<T*>(object);
            };

            lua_newtable(m_state);
            luaL_newmetatable(m_state, e.metatable_name.c_str());
            lua_pushlightuserdata(m_state, &e);
            lua_pushvalue(m_state, -3);
            lua_pushcclosure(m_state, &guarded<&entry_index>, 2);
            lua_setfield(m_state, -2, "__index");
            install_metamethod("__newindex", &guarded<&entry_new_index>, e);
            install_metamethod("__gc", &entry_destructor, e);
            lua_pushlstring(m_state, name.data(), name.size());
            lua_setfield(m_state, -2, "__metatable");
            lua_pop(m_state, 1);
            set_global(m_state, name.c_str());

            m_global_names[name] |= TYPE;
            if ((have & FUNCTION) != 0U) {
                make_type_callable(name.c_str(), &m_registry->global_functions[name]);
            }
        }

        template <typename T, typename V>
        void register_variable(const char* key, V T::* member) {
            add_variable(entry<T>(), key, member);
        }

        template <typename T, typename... Args>
            requires Class<T> && std::constructible_from<T, Args...>
        void register_constructor(const char* key) {
            TypeEntry& e = entry<T>();
            LuaFn& slot = e.constructors[key];
            slot = [ent = &e](lua_State* state) {
                return construct<T, Args...>(state, ent, std::index_sequence_for<Args...>{});
            };
            install_dispatch(e, key, &slot);
        }

        template <typename T, typename R, typename... Args>
        void register_method(const char* key, R (T::*method)(Args...)) {
            register_method_impl<T, R, Args...>(key, method);
        }
        template <typename T, typename R, typename... Args>
        void register_method(const char* key, R (T::*method)(Args...) const) {
            register_method_impl<T, R, Args...>(key, method);
        }

        template <Class T>
        int push_object(T* object) {
            if (find_entry<T>(m_state) == nullptr) {
                report_error(m_state,
                             "adh::script: pushing an object of an unregistered type, call register_type<T>() first");
                return 0;
            }
            new_boxed<T>(m_state, object, false);
            return 1;
        }

        template <Class T>
        void set_global_object(const char* name, T* object) {
            if (find_entry<T>(m_state) == nullptr) {
                report_error(m_state,
                             "adh::script: set_global_object for an unregistered type, call register_type<T>() first");
                return;
            }
            if (!claim_exclusive(name)) {
                return;
            }
            push_object(object);
            set_global(m_state, name);
        }

        void register_raw_function(const char* name, lua_CFunction function) {
            if (!claim_exclusive(name)) {
                return;
            }
            lua_pushcfunction(m_state, function);
            set_global(m_state, name);
        }

        [[nodiscard]] Script create_script(const char* source) {
            if (luaL_loadstring(m_state, source) != LUA_OK) {
                report_error(m_state, pop_error(m_state));
                return Script{};
            }
            return make_loaded_script(std::string{ source }, false);
        }

        [[nodiscard]] Script create_script_file(const char* path) {
            if (luaL_loadfile(m_state, path) != LUA_OK) {
                report_error(m_state, pop_error(m_state));
                return Script{};
            }
            return make_loaded_script(std::string{ path }, true);
        }

      private:
        template <typename T>
        TypeEntry& entry() {
            std::unique_ptr<TypeEntry>& slot = m_registry->types[std::type_index(typeid(T))];
            if (!slot) {
                slot = std::make_unique<TypeEntry>();
            }
            return *slot;
        }

        template <typename T, typename R, typename... Args, typename Method>
        void register_method_impl(const char* key, Method method) {
            TypeEntry& e = entry<T>();
            LuaFn& slot = e.methods[key];
            slot = [method, ent = &e](lua_State* state) {
                return call_method<T, R, Args...>(state, ent, method, std::index_sequence_for<Args...>{});
            };
            install_dispatch(e, key, &slot);
        }

        Script make_loaded_script(std::string source, bool is_file) {
            std::string id{ std::to_string(m_next_id++) + "_script" };
            install_sandboxed_chunk(m_state, id);
            return Script{ std::move(id), m_state, std::move(source), is_file };
        }

        void install_metamethod(const char* field, lua_CFunction function, TypeEntry& e) {
            lua_pushlightuserdata(m_state, &e);
            lua_pushcclosure(m_state, function, 1);
            lua_setfield(m_state, -2, field);
        }

        void install_dispatch(TypeEntry& e, const char* key, LuaFn* slot) {
            if (get_global(m_state, e.type_name.c_str()) != LUA_TTABLE) {
                lua_pop(m_state, 1);
                report_error(m_state, std::string{ "adh::script: cannot install '" } + key + "' on type '" +
                                          e.type_name +
                                          "': its global is not a type table (unregistered or overwritten)");
                return;
            }
            lua_pushstring(m_state, key);
            lua_pushlightuserdata(m_state, slot);
            lua_pushcclosure(m_state, &guarded<&entry_invoke>, 1);
            lua_rawset(m_state, -3);
            lua_pop(m_state, 1);
        }

        std::uint32_t role_of(const char* name) const {
            const auto it{ m_global_names.find(name) };
            return it != m_global_names.end() ? it->second : 0U;
        }

        bool claim_exclusive(const char* name) {
            if (role_of(name) != 0U) {
                report_error(m_state, std::string{ "adh::script: global name '" } + name +
                                          "' is already registered; ignoring the duplicate");
                return false;
            }
            m_global_names[name] = EXCLUSIVE;
            return true;
        }

        void make_type_callable(const char* name, LuaFn* slot) {
            if (get_global(m_state, name) != LUA_TTABLE) {
                lua_pop(m_state, 1);
                return;
            }
            if (lua_getmetatable(m_state, -1) == 0) {
                lua_newtable(m_state);
            }
            lua_pushlightuserdata(m_state, slot);
            lua_pushcclosure(m_state, &guarded<&entry_invoke_as_call>, 1);
            lua_setfield(m_state, -2, "__call");
            lua_setmetatable(m_state, -2);
            lua_pop(m_state, 1);
        }

        template <typename F, typename R, typename... Args>
        LuaFn* store_callable(const char* name, F function, R (* /*unused*/)(Args...)) {
            LuaFn& slot = m_registry->global_functions[name];
            slot = [fn = std::move(function)](lua_State* state) mutable {
                return call_callable<R, Args...>(state, fn, std::index_sequence_for<Args...>{});
            };
            return &slot;
        }

        template <typename R, typename... Args, typename F, std::size_t... I>
        static int call_callable(lua_State* state, F& function, std::index_sequence<I...> /*unused*/) {
            if (!valid_args<Args...>(state, 1)) {
                report_error(state, "adh::script: a registered function was called with an argument of the wrong type");
                return 0;
            }
            if constexpr (std::is_void_v<R>) {
                function(get_type<ArgType<Args>>(state, 1 + static_cast<int>(I))...);
                return 0;
            } else {
                decltype(auto) result = function(get_type<ArgType<Args>>(state, 1 + static_cast<int>(I))...);
                push_result<R>(state, std::forward<decltype(result)>(result), static_cast<int>(sizeof...(Args)));
                return 1;
            }
        }

        void close() noexcept {
            if (m_state != nullptr) {
                lua_close(m_state);
                m_state = nullptr;
            }
        }

        static constexpr std::uint32_t FUNCTION = 1U;
        static constexpr std::uint32_t TYPE = 2U;
        static constexpr std::uint32_t EXCLUSIVE = 4U;

        lua_State* m_state{ nullptr };
        std::unique_ptr<Registry> m_registry;
        StringMap<std::uint32_t> m_global_names;
        std::uint64_t m_next_id{ 0 };
    };

    [[nodiscard]] inline State new_state() {
        lua_State* state{ luaL_newstate() };
        if (state != nullptr) {
            luaL_openlibs(state);
        }
        return State{ state };
    }
} // namespace adh::script
