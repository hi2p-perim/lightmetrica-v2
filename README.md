
Lightmetrica
====================

Lightmetrica : A modern, research-oriented renderer

**This project is WIP**

Project site: http://lightmetrica.org

Build
--------------------

Disclaimer: Currently tested only on Visual Studio 2015 environment.

> BOOST_ROOT="" BOOST_INCLUDEDIR="C:\boost\boost_1_59_0" BOOST_LIBRARYDIR="C:\boost\boost_1_59_0\lib64-msvc-14.0" TBB_ROOT="`pwd`/../external/src/tbb44_20150928oss" TBB_ARCH_PLATFORM="intel64" cmake -G "Visual Studio 14 2015 Win64" ..

Discussions
--------------------

Discussion on possible features, future developments, 
design or achitectural problems, etc.

- Build environment
    + Support MinGW compiler
    + Build tests with Linux environment
- Support code coverage
    + Support on Windows environment
    + Support on Linux environment
- Add portability test on component
    + Compile library with different compiler and check if they works propertly
    + How to automate build and tests? Possibly with travis?
- One problem for current implementation (v1) is that we cannot do a test with high and low precision floating point numbers
  because the precision is controlled by compiler level constants.
    + In order to resolve this, all the functions defined need to be templated.
      However this is not acceptable in terms of implementation.
    + Experiments with changing precision is not feasible.
- Math library
    + Use policy-based design to select optimized or non-optimized version
    + Try to use user defined literals for representing internal floating point type (Math::Float)
- Handling of command line arguments
- Testing
    + Unittest of math library is not sufficient
    + Portability test
        * Create each component with different compiler, and try to run tests with various combinations
    + Performance test
    + Statistics test
- Program options
    + Seprate scene specic options and framework specific options
    + Make modifiable with replacable template
        * This is useful for experiment with various sets of parameters
- Object system
    + Portable property representation
    + Directly convertable from hierarchies in scene file
- Serialization support
    + Makes it possible to print intermediate states, useful for verification or debugging.
- Scene description
    + Possibility to integrate string template
    + Include another file
    + Binary support
- Show defined macros in the startup
- Write an article on how to make things portable
- Remove indices in the interface definition of the component using constexpr counter
    + http://b.atch.se/posts/constexpr-counter/

Recommended practices (C++)
--------------------

This section explains some coding practices specific to this project.
We introcues some coding practices necessary to implement
robust application suitable to the *research-oriented* application.
Some topics says the coding convension (as [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html), which basically depends on)
and some says useful coding practices for extending or reading the implementation.

The practices are based on our experience of developing research-oriented applications. 
Some practices might be uncommon for usual software development,
however we find them effective for constructing research-oriented applications.
Although they are not mandatory, 
we recommend to follow the practices described in this section.

### File name

- The file name should be separated by underbar (_).
    + e.g., `some_header_name.h`

### Project name

- The project name should be separated by hyphen (-).
    + e.g., `lightmetrica-test`

### File headers

- All header and implementation file should include the license header.
- The header file should begin with `#pragma once` and should *not* include addtional include guards.
    + Rationale
        * Most modern compilers support `#pragma once`.
        * Lack of `#endif` in traditional include guards causes tricky bugs.

### Macro

- All macros should begin with `LM_` prefix
    + e.g., `LM_SAFE_DELETE`

### Namespace

- All application specic implementation should be wrapped with `LM_NAMESPACE_BEGIN` and `LM_NAMESPACE_END` macros

### Header files

- Headers files must not be order-dependent, so that the user of the header do not have to care about the include order.
    + That is, include all dependencies in the header files

### Commenting

- Use `#pragma region` as possible
    + To explain the code structure hierarchically
    + cf. [Literate programming](https://en.wikipedia.org/wiki/Literate_programming).

### Code organization

This section contains some *uncommon* practices.
The main motto for the following practice to increase the *locality*
of the implementation.

- Avoid to create a function that is called only once
    + Instead, use scope wrapped by `#pragma region` and `#pragma endregion`
- Prefer *local lambda function* to *private member function*

### Optimization

This is a topic specific to the `research-oriented` application.

- The implementation should not be optimized from the first time
- If you are to optimize the application, be sure to implement 2 versions and check consistency between two results, and measure performance improvement.
- Minimize the number of states or member variables
    + You should not introduce unnecessary state variables for a small optimization
    + Specially in the non-optimized version, the number of cached values should be zero.

### Binary boundary

- All interface should support compiler-level portability
    + Use builtin component system
- All data structure should be POD type

### Tab size

- The tab should be by 4 spaces. Avoid to use `\t` character.

### Function

- The function declaration should use `trailing return types` supported from C++11.
    + Consistent handling of return types
    + Keeping visually arranged looking of the code
- The return type of the lambda function should be specified

### Enums

- The internal type of `enum class` should always be specified.
- Ordinary `enum` type should be wrapped with a namespace

Recommended practices (CMake)
--------------------

We also have the recommneded practice for writing CMake scripts.

### Variables

- The local variable should begin with underbar (_).
    + e.g., ``_SOURCE_FILES``

### Project and library/executable name

- The project and library/executable names should be accessed via variables
    + For project name, use `${PROJECT_NAME}`
    + For library or executable name, define and use local variable `${_PROJECT_NAME}`
        * e.g.,
            - `set(_PROJECT_NAME "lightmetrica-test")`
            - `pch_add_executable(${_PROJECT_NAME} ...)`
