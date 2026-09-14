# Unreal Engine C++ Coding Guideline

Use these rules when writing or editing Unreal Engine C++ code. Keep changes small, readable, and consistent with the surrounding code.

Reference: https://dev.epicgames.com/documentation/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine

## Project Rules

- Prefix project-owned code identifiers with `FE_` when creating new code.
- Work only inside the `LMK/Public` and `LMK/Private` folders for personal LMK code.
- Do not place in-progress LMK code directly in shared folders outside `LMK`.

## UE 5.8 Modern Architecture Standards

- **Gameplay Ability System (GAS)**: All new combat, skill, and stat logic must be implemented via GAS. Do not build custom damage or stat components unless absolutely necessary.
- **Enhanced Input System (EIS)**: Action/Axis mappings are deprecated. Always use `UInputAction` and `UInputMappingContext`.
- **UI & MVVM**: Avoid hard-referencing `UWidget` with `BindWidget`. Use the **UMG Viewmodel (MVVM)** plugin to bind data to views seamlessly.
- **Iris Replication**: Design replicated variables with Iris in mind. Minimize RPC spam, pack data efficiently, and rely on GAS replication for abilities.
- **Asset Management (Soft References)**: Do not use hard class pointers (`TSubclassOf`) or object pointers (`UTexture2D*`) in base classes. Use `TSoftClassPtr` or `TSoftObjectPtr` and load them asynchronously via the `UAssetManager`.

## Core Rules

- Prefer clarity over cleverness.
- Match existing project and Unreal Engine style.
- Do not add speculative abstractions, options, or features.
- Keep public APIs easy to read and hard to misuse.
- Fix compiler warnings instead of ignoring them.
- Avoid unrelated refactors and broad formatting churn.

## Naming

- Use PascalCase for types, functions, and variables.
- Do not use underscores in normal identifiers.
- Use descriptive names; avoid vague names like `Data`, `Info`, `Manager`, or `Check` unless the context is specific.
- Prefix Unreal types correctly:
  - `U` for `UObject` types.
  - `A` for `AActor` types.
  - `S` for Slate widgets.
  - `I` for interfaces.
  - `T` for templates.
  - `E` for enums.
  - `F` for most structs and non-UObject classes.
- Prefix bool variables with `b`, such as `bIsVisible`.
- Name bool functions as clear questions, such as `IsValid()` or `ShouldTick()`.
- Prefix output parameters with `Out`; for bool output parameters, use `bOut`.
- Use `UE_` and all caps for Unreal-related macros.

## Types

- Use Unreal fixed-width types when size matters: `int32`, `uint32`, `uint8`, `int64`.
- Do not use `BOOL`.
- Do not assume the size of `bool`, `TCHAR`, or pointer-sized types.
- Use `UObject*` instead of `UObject&`.
- Prefer `enum class` for enums.
- Use `ENUM_CLASS_FLAGS` for bitmask enum classes.

## Const Correctness

- Use `const` for inputs that are not modified.
- Mark member functions `const` when they do not mutate object state.
- Use `const` references in read-only loops and parameters.
- Do not mark values `const` if they must be moved.
- Do not return `const` by value.

## Modern C++

- Use `nullptr`, not `NULL` or `0`.
- Use `static_assert` for compile-time checks.
- Use both `virtual` and `override` on overridden virtual functions.
- Use `auto` only when the type is obvious or excessively verbose.
- Use lambdas only for short local behavior; keep captures explicit and minimal.
- Use default member initializers when they make construction clearer.

## Formatting

- Put braces on their own lines.
- Do not add a space between a function name and `(`.
- Attach `*` and `&` to the type: `FItem* Item`, `const FString& Name`.
- Use `TEXT()` for string literals.
- Split complex conditions into named bool variables.
- Replace unclear literal arguments with named constants.
- Keep declarations close to first use.

## Headers and Dependencies

- Minimize header includes.
- Prefer forward declarations when possible.
- Put includes in `.cpp` files unless the header truly requires them.
- Do not add inline or template code unless it is justified.
- Keep physical dependencies small to reduce build cost.

## API Design

- Avoid bool flag parameters when they change behavior.
- Use an `enum class` or options struct for multiple modes.
- A bool parameter is acceptable for direct state setters, such as `SetEnabled(bool bEnabled)`.
- Avoid long parameter lists; group related values into a struct.
- Avoid overloads that can be called ambiguously.
- Make ownership and mutation obvious from the signature.

## Comments

- Explain intent, constraints, or non-obvious behavior.
- Do not comment obvious statements.
- Do not use comments to compensate for unclear names or overcomplicated code.
- Keep comments accurate when changing code.

## Platform Code

- Keep platform-specific code in platform-specific files when possible.
- Avoid spreading `PLATFORM_*` checks through general code.
- Prefer feature-based defines over platform-name checks.
- Hide platform differences behind HAL-style abstractions when practical.

## Review Checklist

- Names follow Unreal prefixes and PascalCase.
- Bool names and bool-returning functions read clearly.
- `const`, `override`, `nullptr`, and `TEXT()` are used correctly.
- Header dependencies are minimal.
- APIs avoid ambiguous bool flags and long parameter lists.
- Platform-specific logic is isolated.
- Comments explain why, not what.
- The change is limited to the requested behavior.
