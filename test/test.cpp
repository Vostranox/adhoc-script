#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <print>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <adh/script.hpp>

namespace {

    struct Vec2 {
        float x{};
        float y{};
        Vec2() = default;
        Vec2(float px, float py)
            : x{ px }
            , y{ py } {}
        [[nodiscard]] float length2() const {
            return (x * x) + (y * y);
        }
        void add_x(float delta) {
            x += delta;
        }
        [[nodiscard]] float get_y() const {
            return y;
        }
        [[nodiscard]] Vec2 plus(const Vec2& other) const {
            return Vec2{ x + other.x, y + other.y };
        }
    };

    struct Counted {
        inline static std::int32_t alive = 0;
        std::int32_t id{};
        Counted() {
            ++alive;
        }
        explicit Counted(std::int32_t i)
            : id{ i } {
            ++alive;
        }
        Counted(const Counted& other)
            : id{ other.id } {
            ++alive;
        }
        ~Counted() {
            --alive;
        }
        [[nodiscard]] std::int32_t get_id() const {
            return id;
        }
    };

    struct Body {
        Vec2 position;
        Vec2& position_ref() {
            return position;
        }
        Vec2* position_ptr() {
            return &position;
        }
    };

    struct World {
        Vec2& position_of(Body& body) {
            return body.position;
        }
    };

    struct Outer {
        Body inner;
    };

    struct Named {
        std::string label;
    };

    struct Person {
        std::string name;
        std::int32_t age{};
        [[nodiscard]] std::string greeting() const {
            return "hi " + name;
        }
        void set_name(std::string n) {
            name = std::move(n);
        }
        void set_name_ref(const std::string& n) {
            name = n;
        }
    };

    struct Greeter {
        std::string last;
        void remember(std::string_view name) {
            last = name;
        }
        [[nodiscard]] std::string_view get_last() const {
            return last;
        }
    };

    struct Widget {
        std::int32_t v{};
    };
    struct Dual {
        std::int32_t a{};
        std::int32_t b{};
    };

    std::int32_t sub(std::int32_t a, std::int32_t b) {
        return a - b;
    }
    std::int32_t combine(std::int32_t a, std::int32_t b, std::int32_t c) {
        return (a * 100) + (b * 10) + c;
    }
    std::int32_t length(std::string_view s) {
        return static_cast<std::int32_t>(s.size());
    }
    std::int32_t length_ref(const std::string& s) {
        return static_cast<std::int32_t>(s.size());
    }
    float area(Vec2 v) {
        return v.x * v.y;
    }

    struct ViewHolder {
        std::string_view sv{ "hello" };
        const char* cstr{ "hello" };
    };

    struct Parser {
        std::int32_t value{};
        explicit Parser(const std::string& text)
            : value{ std::stoi(text) } {}
        [[nodiscard]] std::int32_t parse(const std::string& text) const {
            return std::stoi(text);
        }
    };

    struct Tiny {
        char c{};
    };

    struct Unregistered {
        std::int32_t v{ 7 };
    };

    Unregistered make_unregistered() {
        return Unregistered{};
    }

    std::string with_embedded_nul() {
        return std::string("a\0b", 3);
    }

    std::int32_t mul_noexcept(std::int32_t a, std::int32_t b) noexcept {
        return a * b;
    }

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-qualifiers"
#endif
    const bool always_true() {
        return true;
    }
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

    std::int32_t twice(const std::int32_t& v) {
        return v * 2;
    }

    float clamp_ref(const float& v, const float& lo, const float& hi) {
        return std::clamp(v, lo, hi);
    }

    struct Tuning {
        bool on{};
        std::string label;
        float gain{ 1 };
        explicit Tuning(bool b)
            : on{ b } {}
        explicit Tuning(std::string l)
            : label{ std::move(l) } {}
        explicit Tuning(float g)
            : gain{ g } {}
        void scale(const float& factor) {
            gain *= factor;
        }
        void rename(const std::string& text, const bool& enable) {
            label = text;
            on = enable;
        }
    };

    struct Player {
        std::int32_t hp{ 42 };
        bool alive{ false };
        [[nodiscard]] const std::int32_t& get_hp() const {
            return hp;
        }
        [[nodiscard]] const bool& is_alive() const {
            return alive;
        }
        std::int32_t& hp_ref() {
            return hp;
        }
    };

    Vec2* find_nothing() {
        return nullptr;
    }

    float len2_ref(const Vec2& v) {
        return v.length2();
    }

    bool is_null(const Vec2* v) {
        return v == nullptr;
    }

    struct Anchor {
        Vec2 at;
        explicit Anchor(const Vec2& v)
            : at{ v } {}
    };

    struct Handle {
        std::unique_ptr<std::int32_t> value{ std::make_unique<std::int32_t>(7) };
    };

    Handle make_handle() {
        return Handle{};
    }

    struct HandleFactory {
        Handle make() {
            return Handle{};
        }
    };

    std::int32_t length_cstr(const char* s) {
        std::int32_t n{};
        while (s != nullptr && s[n] != '\0') {
            ++n;
        }
        return n;
    }

    int raw_sum(lua_State* L) {
        lua_pushinteger(L, lua_tointeger(L, 1) + lua_tointeger(L, 2));
        return 1;
    }

    std::int32_t s_checks{};
    std::int32_t s_fail{};

    void check(bool cond, std::source_location loc = std::source_location::current()) {
        ++s_checks;
        if (!cond) {
            std::println("  FAIL  {}:{}", loc.file_name(), loc.line());
            ++s_fail;
        }
    }

    using namespace adh;

    script::State make_state() {
        auto state = script::new_state();
        state.register_function("sub", &sub);
        state.register_function("combine", &combine);
        state.register_function("area", &area);
        state.register_type<Vec2>("Vec2");
        state.register_constructor<Vec2, float, float>("new");
        state.register_variable("x", &Vec2::x);
        state.register_variable("y", &Vec2::y);
        state.register_method("length2", &Vec2::length2);
        state.register_method("add_x", &Vec2::add_x);
        state.register_method("get_y", &Vec2::get_y);
        state.register_method("plus", &Vec2::plus);
        state.register_type<Body>("Body");
        state.register_constructor<Body>("new");
        state.register_variable("position", &Body::position);
        return state;
    }

    script::Script loaded(script::State& state, const char* source) {
        auto script = state.create_script(source);
        script.run();
        return script;
    }

    struct ErrorCapture {
        std::string text;
        ErrorCapture() {
            script::set_error_handler([this](const std::string& message) {
                text = message;
            });
        }
        ~ErrorCapture() {
            script::set_error_handler([](const std::string&) {});
        }
        ErrorCapture(const ErrorCapture&) = delete;
        ErrorCapture& operator=(const ErrorCapture&) = delete;
        void clear() {
            text.clear();
        }
        [[nodiscard]] bool has(std::string_view needle) const {
            return text.find(needle) != std::string::npos;
        }
    };

    void test_argument_order() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Run()
                sub2     = sub(10, 3)
                sub3     = sub(3, 10)
                combine1 = combine(1, 2, 3)
                combine2 = combine(7, 8, 9)
            end)");
        script.call("Run");
        check(script.get_number("sub2") == 7);
        check(script.get_number("sub3") == -7);
        check(script.get_number("combine1") == 123);
        check(script.get_number("combine2") == 789);
    }

    void test_type_construct_and_members() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Run()
                local v = Vec2.new(3, 4)
                len2 = v:length2()
                v.x = 10
                xField = v.x
                yField = v.y
            end)");
        script.call("Run");
        check(script.get_number("len2") == 25);
        check(script.get_number("xField") == 10);
        check(script.get_number("yField") == 4);
    }

    void test_class_arg_and_return() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Run()
                local c = Vec2.new(2, 3):plus(Vec2.new(10, 20))
                cx = c.x
                cy = c.y
                a = area(Vec2.new(3, 4))
            end)");
        script.call("Run");
        check(script.get_number("cx") == 12);
        check(script.get_number("cy") == 23);
        check(script.get_number("a") == 12);
    }

    void test_borrowed_global_object() {
        auto state = make_state();
        Vec2 shared{ 1, 2 };
        state.set_global_object("shared", &shared);
        auto script = loaded(state, R"(
            function Run()
                shared.x = 99
                sharedY = shared.y
            end)");
        script.call("Run");
        check(shared.x == 99.0F);
        check(script.get_number("sharedY") == 2);
    }

    void test_class_member_variable() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Run()
                local b = Body.new()
                b.position.x = 7
                b.position.y = 9
                px = b.position.x
                py = b.position.y
                b.position = Vec2.new(3, 4)
                ax = b.position.x
            end)");
        script.call("Run");
        check(script.get_number("px") == 7);
        check(script.get_number("py") == 9);
        check(script.get_number("ax") == 3);
    }

    void test_raw_function() {
        auto state = make_state();
        state.register_raw_function("raw_sum", &raw_sum);
        auto script = loaded(state, "function Run() rawResult = raw_sum(20, 22) end");
        script.call("Run");
        check(script.get_number("rawResult") == 42);
    }

    template <std::size_t... I>
    std::int32_t count_arguments(script::Script& script, std::index_sequence<I...>) {
        return script.call<std::int32_t>("Count", static_cast<std::int32_t>(I)..., Vec2{ 1, 2 });
    }

    void test_many_arguments() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Count(...)
                local n = select("#", ...)
                return select(n, ...).y == 2 and n or -1
            end)");
        check(count_arguments(script, std::make_index_sequence<59>{}) == 60);
        check(lua_gettop(state.handle()) == 0);
    }

    void test_bind_once_call_many() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function A() aRan = (aRan or 0) + 1 end
            function B() bRan = (bRan or 0) + 1 end)");
        script.bind();
        script.call_bound("A");
        script.call_bound("B");
        script.call_bound("A");
        script.unbind();
        check(script.get_number("aRan") == 2);
        check(script.get_number("bRan") == 1);
    }

    void test_object_argument() {
        auto state = make_state();
        auto script = loaded(state, "function Bump(v) v.x = v.x + 100 end");
        Vec2 v{ 1, 2 };
        script.call("Bump", &v);
        check(v.x == 101.0F);
        script.call("Bump", v);
        check(v.x == 201.0F);
    }

    void test_stateful_callable() {
        auto calls = std::make_shared<std::int32_t>(0);
        {
            auto state = make_state();
            state.register_function("next", [calls, value = std::int32_t{ 40 }]() mutable {
                ++*calls;
                return ++value;
            });
            check(calls.use_count() == 2);
            auto script = loaded(state, "function Run() first = next(); second = next() end");
            script.call("Run");
            check(script.get_number("first") == 41);
            check(script.get_number("second") == 42);
            check(*calls == 2);
        }
        check(calls.use_count() == 1);
    }

    template <typename F>
    concept Registrable =
        requires(script::State& state, F function) { state.register_function("F", std::move(function)); };

    void test_callable_must_be_copyable() {
        const auto shared = [value = std::make_shared<std::int32_t>(42)] {
            return *value;
        };
        using MoveOnly = decltype([value = std::make_unique<std::int32_t>(42)] {
            return *value;
        });
        static_assert(Registrable<decltype(shared)> && Registrable<decltype(&mul_noexcept)>);
        static_assert(!Registrable<MoveOnly>);
        auto state = make_state();
        state.register_function("answer", shared);
        auto script = loaded(state, "function Run() return answer() end");
        check(script.call<std::int32_t>("Run") == 42);
    }

    void test_call_return_value() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Compute(a, b) return a * b end
            function Name() return "ada" end
            function Even(n) return n % 2 == 0 end
            function MakeVec(x, y) made = Vec2.new(x, y); return made end
            function Nothing() end)");
        check(script.call<std::int32_t>("Compute", 6, 7) == 42);
        check(script.call<std::string>("Name") == "ada");
        check(script.call<bool>("Even", 4));
        check(!script.call<bool>("Even", 5));
        Vec2* v = script.call<Vec2*>("MakeVec", 3, 4);
        check(v != nullptr);
        check(v->length2() == 25);
        script.call("Nothing");
        check(script.call<std::int32_t>("Missing") == 0);
    }

    template <typename R>
    concept Callable = requires(script::Script& script) { script.call<R>("F"); };

    template <typename R>
    concept BoundCallable = requires(script::Script& script) { script.call_bound<R>("F"); };

    void test_non_owning_call_result() {
        static_assert(Callable<void> && Callable<std::string> && Callable<std::int32_t> && Callable<Vec2*>);
        static_assert(!Callable<std::string_view> && !Callable<const char*> && !Callable<char*>);
        static_assert(BoundCallable<std::string> && !BoundCallable<std::string_view> && !BoundCallable<const char*>);
        auto state = script::new_state();
        auto script = loaded(state, "function Name(n) return string.rep('p', 64) .. n end");
        const std::string name{ script.call<std::string>("Name", 7) };
        lua_gc(state.handle(), LUA_GCCOLLECT);
        check(name == std::string(64, 'p') + "7");
    }

    void test_return_type_mismatch() {
        ErrorCapture errors;
        auto state = make_state();
        auto script = loaded(state, "function NotAVec() return 42 end");
        errors.clear();
        Vec2* v = script.call<Vec2*>("NotAVec");
        check(v == nullptr);
        check(errors.has("unexpected type"));
    }

    void test_field_accessors() {
        auto state = make_state();
        auto script = loaded(state, "function Run() end");
        check(!script.has_number("score"));
        script.set_number("score", 50);
        check(script.has_number("score"));
        check(script.get_number("score") == 50);
        script.set_string("title", "hello");
        check(script.get_string("title") == "hello");
        script.set_bool("ready", true);
        check(script.get_bool("ready"));
        script.set_bool("ready", false);
        check(!script.get_bool("ready"));
        check(script.get_string("absent").empty());
        check(!script.get_bool("absent"));
    }

    void test_lifetime() {
        Counted::alive = 0;
        {
            Counted borrowed{ 42 };
            auto state = script::new_state();
            state.register_type<Counted>("Counted");
            state.register_constructor<Counted, std::int32_t>("new");
            state.register_method("get_id", &Counted::get_id);
            state.set_global_object("borrowed", &borrowed);
            check(Counted::alive == 1);
            auto script = loaded(state, R"(
                function MakeAndDrop()
                    local a = Counted.new(1)
                    local b = Counted.new(2)
                    borrowedId = borrowed:get_id()
                end
                function ForceGC() collectgarbage("collect") end)");
            script.call("MakeAndDrop");
            script.call("ForceGC");
            check(script.get_number("borrowedId") == 42);
            check(Counted::alive == 1);
        }
        check(Counted::alive == 0);
    }

    void test_null_global_error_handler() {
        script::set_error_handler(nullptr);
        {
            auto state = make_state();
            auto script = loaded(state, "function Boom() error('kaboom') end");
            script.call("Boom");
            check(script.call<std::int32_t>("Boom") == 0);
        }
        script::set_error_handler([](const std::string&) {});
    }

    void test_exceptions_become_lua_errors() {
        std::string captured;
        auto state = script::new_state();
        state.set_error_handler([&captured](const std::string& message) {
            captured = message;
        });
        state.register_function("parse", [](const std::string& text) {
            return std::stoi(text);
        });
        state.register_type<Parser>("Parser");
        state.register_constructor<Parser, const std::string&>("new");
        state.register_variable("value", &Parser::value);
        state.register_method("parse", &Parser::parse);
        auto script = loaded(state, R"(
            function ViaFunction(text) return parse(text) end
            function ViaConstructor(text) return Parser.new(text).value end
            function ViaMethod(text) return Parser.new("1"):parse(text) end
            function Caught(text)
                local ok, message = pcall(parse, text)
                return not ok and message
            end
            function Ok() return 42 end)");
        bool all_reported{ true };
        for (std::int32_t i{}; i < 250; ++i) {
            for (const char* function : { "ViaFunction", "ViaConstructor", "ViaMethod" }) {
                captured.clear();
                all_reported = all_reported && script.call<std::int32_t>(function, "not a number") == 0 &&
                               captured.find(function) != std::string::npos &&
                               captured.find("stoi") != std::string::npos;
            }
        }
        check(all_reported);
        check(script.call<std::string>("Caught", "x").find("stoi") != std::string::npos);
        check(script.call<std::int32_t>("ViaMethod", "12") == 12);
        check(script.call<std::int32_t>("Ok") == 42);
        check(lua_gettop(state.handle()) == 0);
    }

    void test_throwing_error_handler() {
        const auto path{ std::filesystem::temp_directory_path() / "adh_throwing_handler_test.lua" };
        std::ofstream{ path } << "function Value() return 1 end";
        auto state = make_state();
        state.set_error_handler([](const std::string& message) {
            throw std::runtime_error(message);
        });
        auto script = state.create_script(R"(
            function Fail() error('boom') end
            function NotAVec() return 42 end
            function WrongArg() return area('not a vector') end)");
        script.run();
        auto failing_chunk = state.create_script("error('at load')");
        auto file_script = state.create_script_file(path.string().c_str());
        std::ofstream{ path } << "function Value( syntax error";
        const auto attempt = [](auto&& action) {
            try {
                action();
            } catch (const std::runtime_error&) {
                return 1;
            }
            return 0;
        };
        lua_State* raw{ state.handle() };
        std::int32_t thrown{};
        bool balanced{ true };
        for (std::int32_t i{}; i < 1000 && balanced; ++i) {
            thrown += attempt([&] {
                script.call("Fail");
            });
            thrown += attempt([&] {
                static_cast<void>(script.call<Vec2*>("NotAVec"));
            });
            thrown += attempt([&] {
                script.call("WrongArg");
            });
            thrown += attempt([&] {
                failing_chunk.run();
            });
            thrown += attempt([&] {
                static_cast<void>(file_script.reload());
            });
            thrown += attempt([&] {
                static_cast<void>(state.create_script("function ("));
            });
            script.bind();
            thrown += attempt([&] {
                script.call_bound("Fail");
            });
            script.unbind();
            balanced = lua_gettop(raw) == 0;
        }
        check(balanced);
        check(thrown == 7000);
        std::filesystem::remove(path);
    }

    void test_error_handler() {
        ErrorCapture errors;
        auto state = make_state();
        auto script = loaded(state, R"(function Boom() error("kaboom") end)");
        script.call("Boom");
        check(errors.has("kaboom"));
        check(errors.has("Boom"));
        check(errors.has("script"));
    }

    void test_strings() {
        auto state = script::new_state();
        state.register_type<Person>("Person");
        state.register_constructor<Person>("new");
        state.register_variable("name", &Person::name);
        state.register_method("greeting", &Person::greeting);
        state.register_method("set_name", &Person::set_name);
        state.register_method("set_name_ref", &Person::set_name_ref);
        state.register_type<Greeter>("Greeter");
        state.register_constructor<Greeter>("new");
        state.register_variable("last", &Greeter::last);
        state.register_method("remember", &Greeter::remember);
        state.register_method("get_last", &Greeter::get_last);
        state.register_function("length", &length);
        state.register_function("length_ref", &length_ref);
        auto script = loaded(state, R"(
            function Run()
                p = Person.new()
                p:set_name("bob")
                g = Person.new()
                g.name = p.name
                h = Person.new()
                h:set_name(p:greeting())
                r = Person.new()
                r:set_name_ref("ref")
                n = length("hello")
                m = length_ref("howdy")
                gr = Greeter.new()
                gr:remember("world")
                viewBack = gr:get_last()
            end)");
        script.call("Run");
        check(script.get_object<Person>("p")->name == "bob");
        check(script.get_object<Person>("g")->name == "bob");
        check(script.get_object<Person>("h")->name == "hi bob");
        check(script.get_object<Person>("r")->name == "ref");
        check(script.get_number("n") == 5);
        check(script.get_number("m") == 5);
        check(script.get_object<Greeter>("gr")->last == "world");
        check(script.get_string("viewBack") == "world");
    }

    void test_function_type_same_name() {
        auto run_case = [](script::State& state) {
            state.register_constructor<Widget>("new");
            state.register_variable("v", &Widget::v);
            auto script = loaded(state, R"(
                function Run()
                    called = Widget(5)
                    w = Widget.new()
                    w.v = 3
                end)");
            script.call("Run");
            check(script.get_number("called") == 50);
            check(script.get_object<Widget>("w")->v == 3);
        };
        {
            auto state = script::new_state();
            state.register_function("Widget", [](std::int32_t n) {
                return n * 10;
            });
            state.register_type<Widget>("Widget");
            run_case(state);
        }
        {
            auto state = script::new_state();
            state.register_type<Widget>("Widget");
            state.register_function("Widget", [](std::int32_t n) {
                return n * 10;
            });
            run_case(state);
        }
    }

    void test_error_reporting() {
        ErrorCapture errors;
        auto state = make_state();
        state.register_function("f", [] {});
        errors.clear();
        state.register_function("f", [] {});
        check(errors.has("already registered"));
        auto script = loaded(state, "function Bad() local a = Vec2.new(1, 1); a:plus(42) end");
        errors.clear();
        script.call("Bad");
        check(errors.has("wrong type"));
    }

    void test_object_retrieval() {
        {
            auto state = make_state();
            auto script = loaded(state, "function Run() v = Vec2.new(3, 4); n = 5 end");
            script.call("Run");
            Vec2* v = script.get_object<Vec2>("v");
            check(v != nullptr);
            check(v->x == 3.0F);
            check(v->y == 4.0F);
            v->x = 9;
            check(script.get_object<Vec2>("v")->x == 9.0F);
            check(script.get_object<Vec2>("missing") == nullptr);
            check(script.get_object<Vec2>("n") == nullptr);
            check(script.get_object<Body>("v") == nullptr);
        }
        Counted::alive = 0;
        Counted* taken = nullptr;
        {
            auto state = script::new_state();
            state.register_type<Counted>("Counted");
            state.register_constructor<Counted, std::int32_t>("new");
            state.register_method("get_id", &Counted::get_id);
            auto script = loaded(state, R"(
                function Make() c = Counted.new(77) end
                function Drop() c = nil; collectgarbage("collect") end)");
            script.call("Make");
            check(Counted::alive == 1);
            taken = script.take_object<Counted>("c");
            check(taken != nullptr);
            script.call("Drop");
            check(Counted::alive == 1);
        }
        check(Counted::alive == 1);
        check(taken->get_id() == 77);
        delete taken;
        check(Counted::alive == 0);
    }

    void test_extensible_type() {
        auto state = script::new_state();
        state.register_type<Widget>("Bag", true);
        state.register_constructor<Widget>("new");
        state.register_variable("v", &Widget::v);
        auto script = loaded(state, R"(
            function Run()
                b = Bag.new()
                b.v = 5
                b.dynamic = 42
                b.note = "hi"
                readV = b.v
                readDyn = b.dynamic
                readNote = b.note
                missing = b.nope
            end)");
        script.call("Run");
        check(script.get_number("readV") == 5);
        check(script.get_number("readDyn") == 42);
        check(script.get_string("readNote") == "hi");
        check(!script.has_number("missing"));
        check(script.get_object<Widget>("b")->v == 5);
    }

    void test_reload() {
        ErrorCapture errors;
        const char* path = "/tmp/adh_reload_test.lua";
        auto write_file = [&](const char* body) {
            std::ofstream{ path } << body;
        };

        write_file("function Value() return 1 end");
        auto state = script::new_state();
        auto script = state.create_script_file(path);
        script.run();
        check(script.call<std::int32_t>("Value") == 1);
        check(!script.needs_reload());

        write_file("function Value() return 2 end");
        std::filesystem::last_write_time(path,
                                         std::filesystem::file_time_type::clock::now() + std::chrono::seconds{ 2 });
        check(script.needs_reload());
        check(script.reload());
        script.run();
        check(script.call<std::int32_t>("Value") == 2);
        check(!script.needs_reload());

        write_file("function Value( syntax error");
        check(!script.reload());
        script.run();
        check(script.call<std::int32_t>("Value") == 2);
    }

    void test_failed_load() {
        ErrorCapture errors;
        auto state = make_state();
        errors.clear();
        auto script = state.create_script("function Update( end");
        check(errors.has("Update"));
        check(!script.valid());
        script.run();
        script.call("Update");
        check(script.call<std::int32_t>("Update", 1, 2) == 0);
        check(script.call<Vec2*>("Update") == nullptr);
        script.bind();
        script.call_bound("Update");
        script.unbind();
        check(!script.has_number("x"));
        check(script.get_number("x") == 0);
        check(!script.get_bool("x"));
        check(script.get_string("x").empty());
        script.set_number("x", 1);
        script.set_bool("x", true);
        script.set_string("x", "y");
        check(script.get_object<Vec2>("x") == nullptr);
        check(script.take_object<Vec2>("x") == nullptr);
        check(!script.reload());
        check(!script.needs_reload());
        check(lua_gettop(state.handle()) == 0);
    }

    void test_strict_globals() {
        ErrorCapture errors;
        auto state = make_state();
        script::Script* strict_script{ nullptr };
        state.register_function("setting", [&strict_script](const std::string& key) {
            return strict_script->get_number(key.c_str());
        });
        auto strict = loaded(state, R"(
            function Defined() return 7 end
            function ReadSetting() return setting("volume_with_a_name_long_enough_for_the_heap") end
            setmetatable(_G, {
                __index = function(_, key) error("undeclared global " .. tostring(key), 2) end,
                __newindex = function(_, key) error("frozen global " .. tostring(key), 2) end,
            })
            setmetatable(Vec2, { __newindex = function(_, key) error("sealed " .. tostring(key), 2) end })
            getmetatable(_ENV).__newindex = function(_, key) error("read-only " .. tostring(key), 2) end)");
        strict_script = &strict;
        lua_State* raw{ state.handle() };

        errors.clear();
        strict.call("Missing");
        check(errors.has("undeclared global Missing"));
        check(strict.call<std::int32_t>("Missing") == 0);
        check(strict.call<std::int32_t>("Defined") == 7);
        errors.clear();
        check(!strict.has_number("absent"));
        check(errors.has("undeclared global absent"));
        check(strict.get_number("absent") == 0);
        check(!strict.get_bool("absent"));
        check(strict.get_string("absent").empty());
        check(strict.get_object<Vec2>("absent") == nullptr);
        check(strict.take_object<Vec2>("absent") == nullptr);
        errors.clear();
        strict.set_number("fresh", 1);
        check(errors.has("read-only fresh"));
        strict.set_bool("fresh", true);
        strict.set_string("fresh", "x");
        errors.clear();
        check(strict.call<double>("ReadSetting") == 0);
        check(errors.has("undeclared global volume"));
        check(lua_gettop(raw) == 0);

        state.register_function("late", [] {
            return 5;
        });
        state.register_method("late_y", &Vec2::get_y);
        state.register_type<Tiny>("Tiny");
        state.register_constructor<Tiny>("new");
        Vec2 shared{ 1, 2 };
        state.set_global_object("sharedVec", &shared);
        state.register_raw_function("raw_sum", &raw_sum);
        auto later = loaded(state, R"(
            function Run()
                local tiny = Tiny.new() and 1
                return late() + Vec2.new(1, 2):late_y() + sharedVec.x + raw_sum(1, 1) + tiny
            end)");
        check(later.call<std::int32_t>("Run") == 11);
        check(lua_gettop(raw) == 0);
    }

    void test_chunk_not_in_globals() {
        auto state = script::new_state();
        auto a = state.create_script("owner = 'A'; runs = (runs or 0) + 1");
        auto b = state.create_script(R"(
            leaked = ""
            for k in pairs(_G) do
                if type(k) == "string" and k:find("script") then leaked = leaked .. k end
            end
            for _, id in ipairs({ "0_script", "1_script" }) do
                _G[id] = function() hijacked = true end
            end)");
        b.run();
        a.run();
        check(b.get_string("leaked").empty());
        check(a.get_string("owner") == "A");
        check(a.get_number("runs") == 1);
        check(!a.get_bool("hijacked") && !b.get_bool("hijacked"));
        a.run();
        check(a.get_number("runs") == 2);
    }

    void test_environment_uses_globals_table() {
        {
            script::State state{ luaL_newstate() };
            state.register_function("sub", &sub);
            auto script = loaded(state, "function Run() r = sub(5, 3) end");
            script.call("Run");
            check(script.get_number("r") == 2);
        }
        {
            auto state = script::new_state();
            state.register_function("sub", &sub);
            auto vandal = loaded(state, "_G._G = nil");
            auto script = loaded(state, "function Run() r = sub(5, 3) end");
            script.call("Run");
            check(script.get_number("r") == 2);
        }
    }

    void test_multi_state_independence() {
        auto state_a = script::new_state();
        state_a.register_type<Dual>("Foo");
        state_a.register_constructor<Dual>("new");
        state_a.register_variable("a", &Dual::a);
        state_a.register_function("Val", [] {
            return 1;
        });

        auto state_b = script::new_state();
        state_b.register_type<Dual>("Bar", true);
        state_b.register_constructor<Dual>("new");
        state_b.register_variable("b", &Dual::b);
        state_b.register_function("Val", [] {
            return 2;
        });

        auto sa = loaded(state_a, R"(
            function Run()
                local d = Foo.new()
                d.a = 10
                out = d.a
                fn = Val()
                bar_missing = (Bar == nil)
            end)");
        sa.call("Run");
        check(sa.get_number("out") == 10);
        check(sa.get_number("fn") == 1);
        check(sa.get_bool("bar_missing"));

        auto sb = loaded(state_b, R"(
            function Run()
                local d = Bar.new()
                d.b = 20
                d.dynamic = 99
                out = d.b
                dyn = d.dynamic
                fn = Val()
                foo_missing = (Foo == nil)
            end)");
        sb.call("Run");
        check(sb.get_number("out") == 20);
        check(sb.get_number("dyn") == 99);
        check(sb.get_number("fn") == 2);
        check(sb.get_bool("foo_missing"));
    }

    void test_nonstring_keys() {
        ErrorCapture errors;
        auto state = make_state();
        auto script = loaded(state, R"(
            function Index()
                local v = Vec2.new(1, 2)
                boolIdx  = (v[true] == nil)
                tableIdx = (v[{}] == nil)
                xField   = v.x
            end
            function Assign()
                local v = Vec2.new(1, 2)
                v[true] = 5
            end)");
        script.call("Index");
        check(script.get_bool("boolIdx"));
        check(script.get_bool("tableIdx"));
        check(script.get_number("xField") == 1);
        errors.clear();
        script.call("Assign");
        check(errors.has("not extensible"));
    }

    void test_extensible_nonstring_keys() {
        auto state = script::new_state();
        state.register_type<Widget>("Bag", true);
        state.register_constructor<Widget>("new");
        auto script = loaded(state, R"(
            function Run()
                local b = Bag.new()
                b[1]   = "int-one"
                b["1"] = "str-one"
                b[true] = "boolean"
                intKey  = b[1]
                strKey  = b["1"]
                boolKey = b[true]
            end)");
        script.call("Run");
        check(script.get_string("intKey") == "int-one");
        check(script.get_string("strKey") == "str-one");
        check(script.get_string("boolKey") == "boolean");
    }

    void test_nested_member_lifetime() {
        auto state = make_state();
        auto script = loaded(state, R"(
            function Run()
                local b = Body.new()
                b.position.x = 7
                b.position.y = 9
                local p = b.position
                b = nil
                collectgarbage("collect")
                collectgarbage("collect")
                survivedX = p.x
                survivedY = p.y
            end)");
        script.call("Run");
        check(script.get_number("survivedX") == 7);
        check(script.get_number("survivedY") == 9);
    }

    void test_method_view_lifetime() {
        auto state = make_state();
        state.register_method("position_ref", &Body::position_ref);
        state.register_method("position_ptr", &Body::position_ptr);
        auto script = loaded(state, R"(
            function Run()
                local r = Body.new():position_ref()
                local p = Body.new():position_ptr()
                r.x = 7
                p.y = 9
                collectgarbage("collect")
                collectgarbage("collect")
                refX = r.x
                ptrY = p.y
            end)");
        script.call("Run");
        check(script.get_number("refX") == 7);
        check(script.get_number("ptrY") == 9);
    }

    void test_argument_view_lifetime() {
        auto state = make_state();
        state.register_function("position_of", [](Body& body) -> Vec2& {
            return body.position;
        });
        state.register_function("position_ptr_of", [](Body* body) {
            return &body->position;
        });
        state.register_type<World>("World");
        state.register_constructor<World>("new");
        state.register_method("position_of", &World::position_of);
        auto script = loaded(state, R"(
            function Run()
                local r = position_of(Body.new())
                local p = position_ptr_of(Body.new())
                local m = World.new():position_of(Body.new())
                r.x = 7
                p.y = 9
                m.x = 11
                collectgarbage("collect")
                collectgarbage("collect")
                refX = r.x
                ptrY = p.y
                methodX = m.x
            end)");
        script.call("Run");
        check(script.get_number("refX") == 7);
        check(script.get_number("ptrY") == 9);
        check(script.get_number("methodX") == 11);
    }

    void test_member_type_validation() {
        ErrorCapture errors;
        auto state = make_state();
        state.register_type<Person>("Person");
        state.register_constructor<Person>("new");
        auto script = loaded(state, R"(
            function Run()
                local b = Body.new()
                b.position.x = 3
                local p = Person.new()
                b.position = p
                keptX = b.position.x
            end)");
        errors.clear();
        script.call("Run");
        check(errors.has("matching type"));
        check(script.get_number("keptX") == 3);
    }

    void test_string_view_member() {
        auto state = script::new_state();
        state.register_type<ViewHolder>("ViewHolder");
        state.register_constructor<ViewHolder>("new");
        state.register_variable("sv", &ViewHolder::sv);
        auto script = loaded(state, "function Run() viewMember = ViewHolder.new().sv end");
        script.call("Run");
        check(script.get_string("viewMember") == "hello");
    }

    void test_non_owning_string_member_assignment() {
        ErrorCapture errors;
        auto state = script::new_state();
        ViewHolder holder;
        state.register_type<ViewHolder>("ViewHolder");
        state.register_variable("sv", &ViewHolder::sv);
        state.register_variable("cstr", &ViewHolder::cstr);
        state.set_global_object("holder", &holder);
        auto script = loaded(state, R"(
            function Assign()
                holder.sv = string.rep("z", 64) .. tostring(os.clock())
                holder.cstr = string.rep("y", 64) .. tostring(os.clock())
            end
            function Read() return holder.sv .. holder.cstr end)");
        errors.clear();
        script.call("Assign");
        check(errors.has("non-owning"));
        lua_gc(state.handle(), LUA_GCCOLLECT);
        check(std::string{ holder.sv } == "hello");
        check(std::string{ holder.cstr } == "hello");
        check(script.call<std::string>("Read") == "hellohello");
    }

    void test_noexcept_function() {
        auto state = script::new_state();
        state.register_function("mul", &mul_noexcept);
        auto script = loaded(state, "function Run() prod = mul(6, 7) end");
        script.call("Run");
        check(script.get_number("prod") == 42);
    }

    void test_const_char_param() {
        auto state = script::new_state();
        state.register_function("clen", &length_cstr);
        auto script = loaded(state, "function Run() clen5 = clen('howdy') end");
        script.call("Run");
        check(script.get_number("clen5") == 5);
    }

    void test_const_bool_return() {
        auto state = script::new_state();
        state.register_function("always", &always_true);
        auto script = loaded(state, R"(
            function Run()
                local v = always()
                kind = type(v)
                asBool = (v == true)
            end)");
        script.call("Run");
        check(script.get_string("kind") == "boolean");
        check(script.get_bool("asBool"));
    }

    void test_scalar_const_ref_params() {
        auto state = script::new_state();
        state.register_function("twice", &twice);
        state.register_function("clamp_ref", &clamp_ref);
        state.register_function("describe", [](const std::string& text, const std::int32_t& n, const char* const tail) {
            return text + std::to_string(n) + tail;
        });
        state.register_type<Tuning>("Tuning");
        state.register_constructor<Tuning, const bool>("from_bool");
        state.register_constructor<Tuning, const std::string>("from_label");
        state.register_constructor<Tuning, const float&>("from_gain");
        state.register_variable("on", &Tuning::on);
        state.register_variable("label", &Tuning::label);
        state.register_variable("gain", &Tuning::gain);
        state.register_method("scale", &Tuning::scale);
        state.register_method("rename", &Tuning::rename);
        auto script = loaded(state, R"(
            function Yes() return true end
            function Run()
                doubled = twice(21)
                clamped = clamp_ref(7.5, 0, 5)
                described = describe("n=", 7, "!")
                flag = Tuning.from_bool(true).on
                label = Tuning.from_label("ada").label
                local t = Tuning.from_gain(2)
                t:scale(1.5)
                gain = t.gain
                t:rename("renamed", true)
                renamed = t.label
                renamedOn = t.on
            end)");
        script.call("Run");
        check(script.get_number("doubled") == 42);
        check(script.get_number("clamped") == 5);
        check(script.get_string("described") == "n=7!");
        check(script.get_bool("flag"));
        check(script.get_string("label") == "ada");
        check(script.get_number("gain") == 3);
        check(script.get_string("renamed") == "renamed");
        check(script.get_bool("renamedOn"));
        check(script.call<const bool>("Yes"));
    }

    void test_scalar_ref_returns() {
        static const float gravity{ 9.5F };
        auto state = script::new_state();
        state.register_function("gravity", []() -> const float& {
            return gravity;
        });
        state.register_type<Player>("Player");
        state.register_constructor<Player>("new");
        state.register_method("get_hp", &Player::get_hp);
        state.register_method("is_alive", &Player::is_alive);
        state.register_method("hp_ref", &Player::hp_ref);
        auto script = loaded(state, R"(
            function Run()
                local p = Player.new()
                hpType = type(p:get_hp())
                hp = p:get_hp() + 1
                aliveType = type(p:is_alive())
                aliveTruthy = p:is_alive() and true or false
                hpRef = p:hp_ref()
                gravityType = type(gravity())
                g = gravity()
            end)");
        script.call("Run");
        check(script.get_string("hpType") == "number");
        check(script.get_number("hp") == 43);
        check(script.get_string("aliveType") == "boolean");
        check(!script.get_bool("aliveTruthy"));
        check(script.get_number("hpRef") == 42);
        check(script.get_string("gravityType") == "number");
        check(script.get_number("g") == 9.5);
    }

    void test_integer_from_fractional_number() {
        auto state = make_state();
        state.register_type<Widget>("Widget");
        state.register_constructor<Widget>("new");
        state.register_variable("v", &Widget::v);
        auto script = loaded(state, R"(
            function Frac() return 9.5 end
            function Huge() return 1e300 end
            function NegHuge() return -1e300 end
            function NaN() return 0 / 0 end
            function Text() return "2.75" end
            function Run()
                positive = sub(2.5, 1)
                negative = sub(-2.5, 1)
                exact = sub(4.0, 1)
                local w = Widget.new()
                w.v = 7.9
                memberPositive = w.v
                w.v = -7.9
                memberNegative = w.v
            end)");
        script.call("Run");
        check(script.get_number("positive") == 1);
        check(script.get_number("negative") == -3);
        check(script.get_number("exact") == 3);
        check(script.get_number("memberPositive") == 7);
        check(script.get_number("memberNegative") == -7);
        check(script.call<std::int32_t>("Frac") == 9);
        check(script.call<std::int64_t>("Huge") == 0);
        check(script.call<std::int64_t>("NegHuge") == 0);
        check(script.call<std::int32_t>("NaN") == 0);
        check(script.call<std::uint8_t>("Text") == 2);
    }

    void test_set_global_object_unregistered() {
        ErrorCapture errors;
        auto state = script::new_state();
        Widget w{ 5 };
        errors.clear();
        state.set_global_object("thing", &w);
        check(errors.has("unregistered type"));
        state.register_function("thing", [] {
            return 99;
        });
        auto script = loaded(state, "function Run() out = thing() end");
        script.call("Run");
        check(script.get_number("out") == 99);
    }

    void test_unregistered_return_type() {
        ErrorCapture errors;
        auto state = script::new_state();
        state.register_function("makeUnreg", &make_unregistered);
        auto script = loaded(state, "function Run() local u = makeUnreg() end");
        errors.clear();
        script.call("Run");
        check(errors.has("unregistered type"));
    }

    void test_null_object_arg() {
        ErrorCapture errors;
        auto state = make_state();
        auto script = loaded(state, R"(
            function ReadField(v)  return v.x end
            function WriteField(v) v.x = 5 end
            function CallMethod(v) return v:length2() end)");
        Vec2* nothing = nullptr;
        errors.clear();
        const double r = script.call<double>("ReadField", nothing);
        check(errors.has("null object"));
        check(r == 0);
        errors.clear();
        script.call("WriteField", nothing);
        check(errors.has("null object"));
        errors.clear();
        script.call<double>("CallMethod", nothing);
        check(errors.has("null"));
    }

    void test_null_object_as_value_arg() {
        ErrorCapture errors;
        auto state = make_state();
        state.register_function("find_nothing", &find_nothing);
        state.register_function("len2_ref", &len2_ref);
        state.register_function("is_null", &is_null);
        state.register_type<Anchor>("Anchor");
        state.register_constructor<Anchor, const Vec2&>("new");
        auto script = loaded(state, R"(
            function ByRef() return len2_ref(find_nothing()) end
            function ByValue() return area(find_nothing()) end
            function AsMethodArg() return Vec2.new(1, 2):plus(find_nothing()) end
            function AsConstructorArg() return Anchor.new(find_nothing()) end
            function AsPointer() return is_null(find_nothing()) end
            function GetNull() return find_nothing() end)");
        for (const char* function : { "ByRef", "ByValue", "AsMethodArg", "AsConstructorArg" }) {
            errors.clear();
            script.call(function);
            check(errors.has("wrong type"));
        }
        check(script.call<bool>("AsPointer"));
        errors.clear();
        const Vec2 v{ script.call<Vec2>("GetNull") };
        check(errors.has("unexpected type"));
        check(v.x == 0.0F && v.y == 0.0F);
        check(script.call<Vec2*>("GetNull") == nullptr);
    }

    void test_move_only_return() {
        auto state = script::new_state();
        state.register_type<Handle>("Handle");
        state.register_function("make_handle", &make_handle);
        state.register_type<HandleFactory>("HandleFactory");
        state.register_constructor<HandleFactory>("new");
        state.register_method("make", &HandleFactory::make);
        auto script =
            loaded(state, "function Run() fromFunction = make_handle(); fromMethod = HandleFactory.new():make() end");
        script.call("Run");
        for (const char* name : { "fromFunction", "fromMethod" }) {
            const Handle* handle{ script.get_object<Handle>(name) };
            check(handle != nullptr && handle->value != nullptr && *handle->value == 7);
        }
    }

    void test_forged_metamethod_calls() {
        ErrorCapture errors;
        auto state = make_state();
        state.register_type<Tiny>("Tiny");
        state.register_constructor<Tiny>("new");
        state.register_function("Tiny", [] {
            return 7;
        });
        auto script = loaded(state, R"(
            function Protected()
                local v = Vec2.new(1, 2)
                local ok = pcall(setmetatable, {}, getmetatable(v))
                return getmetatable(v) == "Vec2" and not ok
            end
            function IndexNil() return debug.getmetatable(Vec2.new()).__index(nil, 'y') end
            function NewIndexOther() debug.getmetatable(Vec2.new()).__newindex(Tiny.new(), 'y', 1.5) end
            function GcOther() debug.getmetatable(Vec2.new()).__gc(Tiny.new()) end
            function GcTable()
                setmetatable({}, debug.getmetatable(Vec2.new()))
                collectgarbage("collect")
                collectgarbage("collect")
            end
            function CallWithoutType() return getmetatable(Tiny).__call() end
            function StillWorks() return Vec2.new(3, 4):length2() + Tiny() end)");
        check(script.call<bool>("Protected"));
        for (const char* function : { "IndexNil", "NewIndexOther" }) {
            errors.clear();
            script.call(function);
            check(errors.has("another type"));
        }
        script.call("GcOther");
        script.call("GcTable");
        errors.clear();
        script.call("CallWithoutType");
        check(errors.has("without the type"));
        check(script.call<std::int32_t>("StillWorks") == 32);
        check(lua_gettop(state.handle()) == 0);
    }

    void test_long_identifiers() {
        auto state = script::new_state();
        state.register_type<Vec2>("Vec2");
        state.register_constructor<Vec2, float, float>("construct_a_new_vector_instance");
        state.register_variable("the_x_coordinate_component_value", &Vec2::x);
        state.register_method("compute_the_squared_length_value", &Vec2::length2);
        state.register_function("subtract_the_second_from_the_first", &sub);
        auto script = loaded(state, R"(
            function Run()
                local v = Vec2.construct_a_new_vector_instance(3, 4)
                lenLong = v:compute_the_squared_length_value()
                local asValue = v.compute_the_squared_length_value
                lenViaValue = asValue(v)
                xLong = v.the_x_coordinate_component_value
                v.the_x_coordinate_component_value = 10
                xLong2 = v.the_x_coordinate_component_value
                fnLong = subtract_the_second_from_the_first(10, 3)
            end)");
        script.call("Run");
        check(script.get_number("lenLong") == 25);
        check(script.get_number("xLong") == 3);
        check(script.get_number("xLong2") == 10);
        check(script.get_number("fnLong") == 7);
        check(script.get_number("lenViaValue") == 25);
    }

    void test_nested_member_views() {
        auto state = script::new_state();
        state.register_type<Vec2>("Vec2");
        state.register_variable("x", &Vec2::x);
        state.register_type<Body>("Body");
        state.register_variable("position", &Body::position);
        state.register_type<Outer>("Outer");
        state.register_constructor<Outer>("new");
        state.register_variable("inner", &Outer::inner);
        auto script = loaded(state, R"(
            function Run()
                local o = Outer.new()
                o.inner.position.x = 42
                local p = o.inner.position
                o = nil
                collectgarbage("collect")
                collectgarbage("collect")
                survived = p.x
            end)");
        script.call("Run");
        check(script.get_number("survived") == 42);
    }

    void test_string_embedded_nul() {
        auto state = script::new_state();
        state.register_function("with_nul", &with_embedded_nul);
        Named obj;
        obj.label = std::string("x\0y\0z", 5);
        state.register_type<Named>("Named");
        state.register_variable("label", &Named::label);
        state.set_global_object("obj", &obj);
        auto script = loaded(state, R"(
            function Run()
                returnLen = #with_nul()
                memberLen = #obj.label
            end)");
        script.call("Run");
        check(script.get_number("returnLen") == 3);
        check(script.get_number("memberLen") == 5);
    }

    void test_string_embedded_nul_from_lua() {
        auto state = script::new_state();
        state.register_function("length_ref", &length_ref);
        state.register_function("length_value", [](std::string s) {
            return static_cast<std::int32_t>(s.size());
        });
        Named obj;
        state.register_type<Named>("Named");
        state.register_variable("label", &Named::label);
        state.set_global_object("obj", &obj);
        auto script = loaded(state, R"(
            data = "ab\0cd"
            function Get() return data end
            function Run()
                byRef = length_ref(data)
                byValue = length_value(data)
                obj.label = data
            end)");
        script.call("Run");
        const std::string expected{ "ab\0cd", 5 };
        check(script.get_number("byRef") == 5);
        check(script.get_number("byValue") == 5);
        check(obj.label == expected);
        check(script.get_string("data") == expected);
        check(script.call<std::string>("Get") == expected);
    }

    void test_concurrent_states() {
        constexpr int kThreads = 8;
        constexpr int kIters = 25;
        std::atomic<int> succeeded{ 0 };
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([t, &succeeded] {
                bool good = true;
                for (int i = 0; i < kIters; ++i) {
                    std::string captured;
                    auto state = make_state();
                    state.set_error_handler([&captured](const std::string& message) {
                        captured = message;
                    });
                    auto adder = loaded(state, "function Sum(x, y) return x + y end");
                    auto boomer = loaded(state, "function Boom() error('kaboom') end");
                    good = good && (adder.call<std::int32_t>("Sum", t, i) == t + i);
                    boomer.call("Boom");
                    good = good && (captured.find("kaboom") != std::string::npos);
                }
                if (good) {
                    succeeded.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        check(succeeded.load() == kThreads);
    }

} // namespace

int main() {
    test_argument_order();
    test_type_construct_and_members();
    test_class_arg_and_return();
    test_borrowed_global_object();
    test_class_member_variable();
    test_raw_function();
    test_bind_once_call_many();
    test_many_arguments();
    test_object_argument();
    test_stateful_callable();
    test_callable_must_be_copyable();
    test_call_return_value();
    test_non_owning_call_result();
    test_return_type_mismatch();
    test_field_accessors();
    test_lifetime();
    test_error_handler();
    test_throwing_error_handler();
    test_exceptions_become_lua_errors();
    test_null_global_error_handler();
    test_strings();
    test_function_type_same_name();
    test_error_reporting();
    test_object_retrieval();
    test_extensible_type();
    test_reload();
    test_failed_load();
    test_strict_globals();
    test_chunk_not_in_globals();
    test_environment_uses_globals_table();
    test_multi_state_independence();
    test_nonstring_keys();
    test_extensible_nonstring_keys();
    test_nested_member_lifetime();
    test_method_view_lifetime();
    test_argument_view_lifetime();
    test_member_type_validation();
    test_string_view_member();
    test_non_owning_string_member_assignment();
    test_noexcept_function();
    test_const_char_param();
    test_const_bool_return();
    test_scalar_const_ref_params();
    test_scalar_ref_returns();
    test_integer_from_fractional_number();
    test_set_global_object_unregistered();
    test_unregistered_return_type();
    test_null_object_arg();
    test_null_object_as_value_arg();
    test_move_only_return();
    test_forged_metamethod_calls();
    test_long_identifiers();
    test_nested_member_views();
    test_string_embedded_nul();
    test_string_embedded_nul_from_lua();
    test_concurrent_states();

    if (s_fail != 0) {
        std::println("FAILED: {} of {} checks", s_fail, s_checks);
        return 1;
    }
    std::println("OK: all {} checks passed", s_checks);
    return 0;
}
