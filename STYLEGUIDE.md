# Staircase Project Style Guide

> **Note**: This style guide is a living document. Decisions regarding code
> style will be logged here as they come up, and are open to future refinement.

## Naming Conventions

### Member Variables

Context should make the role of a variable evident. When context alone is
insufficient, we use the built-in `this->` prefix to explicitly identify member
variables.

#### Rationale

-   Leverage built-in language features for clarity and simplicity.
-   Reduce noise and promote code readability through context rather than
    out-of-band artifacts (i.e. variable prefixes).
-   Excessive use of `this->` serves as a built-in warning system, suggesting that
    the code may lack clarity or the class may be carrying too much state or
    responsibility.

## Modifier Placement

### `const` Modifier

This project prefers the `const` modifier to be placed on the right side of the
type.

#### Rationale

- Facilitates a right-to-left reading of the type (e.g. "reference to a const
  string")

#### Example

Preferred:

```cpp
std::string const &myString;
```

Not preferred:

```cpp
const std::string &myString;
```

#### Supporting References

-   [Stack Overflow Discussion](https://stackoverflow.com/a/1228719/2974621)
-   [C++ Core Guidelines](http://isocpp.github.io/CppCoreGuidelines)
