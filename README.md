
Lightmetrica
====================

Lightmetrica : A modern, research-oriented renderer

[![Build Status](https://travis-ci.org/hi2p-perim/lightmetrica-v2.svg?branch=master)](https://travis-ci.org/hi2p-perim/lightmetrica-v2)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/hi2p-perim/lightmetrica-v2?branch=master&svg=true)](https://ci.appveyor.com/project/HisanariOtsu/lightmetrica-v2)

Project site: http://lightmetrica.org

Documentation
--------------------

- [Documentation](http://lightmetrica.org/doc)
- [Developer's guide](http://lightmetrica.org/doc/dev_guide/)
- [API documentation](http://doc.lightmetrica.org)

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

License
--------------------

This software is distributed under MIT License. For details, see LICENCE file.
