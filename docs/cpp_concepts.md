# C++ Concepts — quick reference

This short note explains common C++ concepts seen in this repo.

## Headers vs source files

- Header (`.hpp` / `.h`): declare interfaces — types, class declarations, function signatures.
- Source (`.cpp`): provide implementations — function bodies and non-inline logic.

Example:

```cpp
// include/book/order_book.hpp
class OrderBook {
public:
  void apply_update(...); // declaration
};

// src/book/order_book.cpp
#include "book/order_book.hpp"

void liquidbook::OrderBook::apply_update(...) { /* implementation */ }
```

Why split? clear interface vs implementation, smaller recompiles, better encapsulation.

## `namespace` and anonymous namespaces

- `namespace liquidbook { ... }` groups symbols to avoid name collisions (same role as `std` for the standard library).
- Don't put `using namespace ...;` in headers — it pollutes every file that includes the header.
- Anonymous namespace in a `.cpp` gives internal linkage (file-local):

```cpp
namespace {
  void helper() { /* visible only in this translation unit */ }
}
```

## `#pragma once`

- Prevents multiple inclusion of the same header in one translation unit.
- Simpler than manual include guards:

```cpp
#ifndef LIQUIDBOOK_ORDER_BOOK_HPP
#define LIQUIDBOOK_ORDER_BOOK_HPP
// ...
#endif
```

- Widely supported by modern compilers.

## Inline functions, `constexpr`, and templates

- Small functions and `constexpr` can be defined in headers (often `inline` to avoid ODR issues).
- Class templates and function templates must be defined in headers so their definitions are available at instantiation time.

## ODR (One Definition Rule)

- ODR means each entity (non-inline function, non-inline variable, class type, enum, etc.) must have exactly one definition in the whole program (across all translation units). Violating it is undefined behavior or a linker error.
- Common cause: putting a non-inline function or a non-inline non-static variable definition in a header that is included by multiple `.cpp` files.

Example (bad — ODR violation):

```cpp
// bad.hpp (included by two .cpp files)
void helper() { /* function body */ } // definition in header -> multiple defs
```

Fixes:

- Move the definition to a `.cpp` and keep only the declaration in the header:
  - `// header: void helper();`
  - `// source: void helper() { ... }`
- Or make it `inline` / `constexpr` / a template / an `inline` variable so identical definitions in multiple TUs are allowed:
  ```cpp
  inline void helper() { /* ok in header */ }
  ```
- Or give it internal linkage so each TU has its own copy:
  ```cpp
  namespace { void helper() { } } // anonymous namespace -> internal linkage
  // or
  static void helper() { } // internal linkage (older style)
  ```

Short rule of thumb: put non-inline implementations in `.cpp`; keep small/inline/template definitions in headers.

## `[[nodiscard]]`

- Attribute that warns when the function's return value is discarded.
- Useful for functions whose result must be checked (e.g. error codes, `std::optional`).

Example:

```cpp
[[nodiscard]] std::optional<double> best_bid() const;
```

If the caller ignores the return value, the compiler will emit a warning.

## `std::optional`

- Wrapper for an object that may be empty.
- Use when a function may not have a value to return instead of sentinel values.
- `std::nullopt` means the optional has no value; it is the explicit "empty" state for `std::optional<T>`.

Example:

```cpp
auto b = book.best_bid();
if (b) {
  double price = *b; // has value
} else {
  // no bid available
}
```

This is exactly why the implementation does:

```cpp
if (bid_levels_.empty())
    return std::nullopt;
```

## `const` member functions

- `T f() const;` means the function doesn't (logically) modify the object and can be called on `const` instances.

Example:

```cpp
std::optional<double> best_bid() const; // doesn't modify `this`
```

## `noexcept`

- `noexcept` marks that a function does not throw exceptions; it can enable optimizations and affects exception propagation behavior.

Example:

```cpp
void clear() noexcept;
```

Combined usage:

```cpp
[[nodiscard]] std::optional<double> best_bid() const noexcept;
```

This says: caller should not ignore the result, the function won't modify `this`, and it won't throw.

---

## `std::string_view`, the `toupper` cast, and `side_from_string`

- `std::string_view` is a non-owning view of characters (pointer + length). It's cheap to pass and avoids copies for read-only text parameters, but it must not outlive the referred storage.

- The character-conversion line in `side_from_string`:

```cpp
upper += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
```

- `std::toupper` takes an `int` that must either be `EOF` or represent an `unsigned char` value. Passing a plain `char` (which may be signed) can cause undefined behavior for negative `char` values on some platforms.
- `static_cast<unsigned char>(c)` converts the `char` to the correct unsigned range; promotion to `int` happens when calling `std::toupper`.
- `std::toupper` returns `int`; casting back to `char` stores the resulting character in `upper`.
- This pattern is the portable, safe way to uppercase characters coming from `char`.

- `side_from_string` expects the textual side name (case-insensitive) and compares the uppercased input to the literals `"BID"` and `"ASK"`.
  - Examples accepted: `"BID"`, `"Bid"`, `"bid"`.
  - Inputs with extra whitespace or other characters (e.g. `" bid "`) will not match and will cause `std::invalid_argument` to be thrown.

Example:

```cpp
Side s = side_from_string("Bid"); // Side::Bid
Side s2 = side_from_string(std::string_view{"ask"}); // Side::Ask
// side_from_string("unknown") -> throws std::invalid_argument
```

## Reverse iterators and `.base()`

- `rbegin()` starts at the last element of a container; `rend()` is the one-before-the-beginning marker.
- A reverse iterator is not the same type as a normal forward iterator, and `std::vector::erase()` needs a forward iterator.
- That is why code like this appears:

```cpp
for (auto it = bid_levels_.rbegin(); it != bid_levels_.rend(); ++it) {
    if (it->price == price) {
        if (qty == 0.0) {
            auto erase_it = std::next(it).base();
            bid_levels_.erase(erase_it);
        }
    }
}
```

- `std::next(it)` advances the reverse iterator by one step.
- `.base()` converts the reverse iterator back to the equivalent normal iterator for the underlying `std::vector`.
- This is a common C++ pattern when erasing from a reverse loop.
