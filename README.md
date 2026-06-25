# adhoc-script

A C++ <-> Lua binding layer in a single header.

I originally wrote this as the scripting layer for a student game-engine project.
I've since extracted it from the engine, fixed some bugs, added
features, and moved it to C++23.

This library wraps Lua's C API so you can expose C++ to scripts directly. It allows you
to register functions, types, and objects by their real signatures, rather than
hand-writing the binding glue for each one. Each script runs in its own sandboxed
environment.

## Usage

```cpp
#include <adh/script.hpp>

struct State {
    bool playing{};
};

struct Vec2 {
    float x{};
    float y{};
    Vec2(float px, float py) : x{ px }, y{ py } {}
    float length2() const { return x * x + y * y; }
};

int add(int a, int b) { return a + b; }

auto state = adh::script::new_state();

state.register_function("add", &add);

state.register_type<Vec2>("Vec2");
state.register_constructor<Vec2, float, float>("new");
state.register_variable<Vec2>("x", &Vec2::x);
state.register_method<Vec2>("length2", &Vec2::length2);

State game{};
state.register_type<State>("State");
state.register_variable<State>("playing", &State::playing);
state.set_global_object("state", &game);

auto script = state.create_script(R"(
    function Update()
        state.playing = true;
        local v = Vec2.new(3, 4)
        result = add(v.x, v:length2())   -- 3 + 25
    end
)");

script.run();
script.call("Update");
double result = script.get_number("result");   // 28
```

## Notes

- Requires C++23 and depends on Lua 5.4.
- Bound functions and error handlers are stored in `std::function`, so they must be copyable.

## Tests

The `test/` folder has a standalone test that exercises the whole public API. From the project root (adjust the Lua include/lib for your system):

```sh
clang++ -Wall -Wextra -std=c++23 -g -O0 -fsanitize=address,undefined -pthread -Iinclude -I/usr/include/lua5.4 test/test.cpp -o run_tests -llua5.4 -lm && ./run_tests
```

The suite includes a multi-threaded test; to also check for data races, build it with
ThreadSanitizer:

```sh
clang++ -Wall -Wextra -std=c++23 -g -O0 -fsanitize=thread -pthread -Iinclude -I/usr/include/lua5.4 test/test.cpp -o run_tests_tsan -llua5.4 -lm && ./run_tests_tsan
```
