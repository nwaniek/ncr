ncr - Neural Computation Repository and Resources
=================================================

This repository is a collection of C++ header only files. The files contain
implementations of various basic functions and data structures that are required
to develop simulations of (evolving) automata and spiking neural networks. While
the headers partially try to follow existing structures and functions from the
C++ STL, they diverge quite drastically at others.


Disclaimer
----------

Q: Are the implementations of the data structures and algorithms complete?
A: Almost surely no.

Q: Are at least all implementations correct?
A: Also almost surely no. Most of the functions were tested and evaluated in
separate projects, but of course there's no guarantee that they are entirely
correct and don't contain some bugs which were not found and squashed yet.
Also, some implementations might not follow some constraints given or
recommended by the C++ standard. Such locations are, however, scarce.

Q: What about speed?
A: While most algorithms were selected to have reasonable space and time
complexity, they were not developed with the aim to have the fastest
implementation around. Their performance currently suffices my own personal
requirements.

Q: Are all the algorithms and data structures stable?
A: Almost surely no. They are used in several fast-moving and experimental
scientific projects. They are thus subject to change. That is, if there are
issues within or limits to their current implementations, they will be
adapated accordingly to fit the needs of these projects. In case of drastic
changes, this will be announced properly or the implementations moved into new
files (for instance see ncr_transport -> ncr_transport2).

Q: Then why use them?
A: Because there were no alternatives available that didn't pull in many
additional dependencies. Also, some of the C++20 headers were not available on
the systems that I compiled my software on. Still, I needed partial
implementations of the stuff. Sometimes the STL headers don't provide all the
methods or access to data that was required during a project (example: specify
the storage type of an std::bitset, or access its underlying array to write it
to a binary file, or change its size during run-time).

Conclusion: Use entirely at your own risk. In case of doubt, test if the
functions are correct and in particular measure performance if this is a
critical assessment criteria. If you find issues, bugs, etc. please open pull
requests. Requests for additional data structures and algorithms are welcome,
ideally with a pull request that implements said stuctures and algorithms.

Another point to consider is that this library is used in projects that
interface partially with plain C projects (C11, C17). Hence, some of the
structures were developed with easy interfacing to C in mind, in particular the
ODE solvers. That is, the current implementation allows an easy port to C17,
without too many adjustments. Because C and C++ are significantly different in
several parts, this is reflected in definitions of structs, types, or functions.
Because interfacing with other projects might change over time (with a stronger
focus on modern C++ projects), this "legacy" style might change accordingly to
more modern implementations.


Usage Guidelines
----------------

There are no explicit rules when using the headers except following the MIT
License (see below, or the LICENSE file). Still, if you use any of the headers
or other parts of the ncr ecosystem in your work, it would be great if you could
credit them either by explicitly referencing this website or
`https://rochus.net`, or even better, cite one of my papers.

If none of my existing papers fits your bill, then you could use for instance
the following (bibtex) snippet:

.. code::
    @Misc{ncr,
        author =   {Nicolai Waniek},
        title =    {{NCR}: {Neural Computation Repository and Resources}},
        howpublished = {\url{https://github.com/nwaniek/ncr}}
        year = {2021--2022}
    }

There might be a proper paper, which describes the software in detail, to cite
in the future. So, stay tuned.

If you want to donate to this project, get in touch with me. Alternatively, tell
your favorite funding agency or company to (continue) fund my research.


Overview
--------

The following gives a brief overview of the 'core' headers in ncr. They can be
found in the folder `include/ncr`.

.. code::
    ncr_algorithm  - certain algorithms to work with ncr data structures
    ncr_automata   - header file for alphabets, strings, and finite state machines
    ncr_bayes      - Bayesian math, structures, and functions, e.g. particle filters
    ncr_bitset     - compile-time fixed as well as run-time dynamic size bitset
                     similar to std::bitset
    ncr_chrono     - time measurement functions
    ncr_cmd        - a simple command (file) parser
    ncr_cvar       - A configuration variable system inspired by id's Quake 3 Arena.
    ncr_filesystem - very basic filesystem utilities
    ncr_geometry   - math operations for geometry processing. Don't use this.
                     Rather, go have a look at Keagan Crane's excellent work!
    ncr_graph      - Algorithms to work with (probabilistic) graphs
    ncr_log        - A logger inspired by articles found on Dr. Dobb's
    ncr_log_stub   - In case you need a dummy interface for ncr_log, but don't want
                     to use ncr_log.
    ncr_math       - Mathematical functions. Don't use this.
    ncr_memory     - A Slab Memory Manager for systems and software in which
                     explicit memory management is required.
    ncr_neuron     - Implementations of several neuron models and populations
                     thereof, such as Izhikevich, AdEx, Hodgkin-Huxley, and
                     plasticity rules such as STDP. The implementations are
                     partially incomplete, in particular the plasticity rules, and
                     will be subject to change soon.
    ncr_numeric    - Numerical methods, in particular for solving systems of ODEs.
                     Includes the most familiar ODE solver such as RK2, RK4,
                     Dormand&Prince (which runs under the hood of Matlab's ode45),
                     etc.
    ncr_random     - Implementations of some random distributions, e.g. von Mises,
                     Laplace, and Poisson
    ncr_simulation - A framework for building simulations that can be either
                     time-based (elapsed time is tracked as a float value based,
                     e.g. on milli-seconds), tick-based (elapsed time is tracked as
                     an unsigned integer that ticks through iterations), or timeless
                     (no time tracking)
    ncr_string     - Some helper functions to work with strings
    ncr_transport  - A communication transport subsystem inspired by Erlang's (and
                     for that sake Hoare's Communicating Sequential Process) message
                     passing between "ports". Don't use this, use ncr_transport2.
    ncr_transport2 - A slight improvement to ncr_transport which externalises
                     comparison of message delivery to the user of ncr_transport
    ncr_units      - Some basic literal specifications to have common grounds in all
                     projects
    ncr_utils      - Helpful utility functions and macros
    ncr_variant    - Helper functions for working with variant data types that are
                     in addition to what is already provided by STL's <variant>
                     header. E.g. a 'visit' function which might be slightly faster
                     than variant's original visit.
    ncr_vector     - A vector implementation that relies on some underlying
                     contiguous memory, but which can be also used to "view" only
                     parts of another existing vector. If compiled with
                     the NCR_USE_BLAS option, will fall back onto BLAS functions as
                     much as possible. If compiled with NCR_VECTOR_MOVE_SEMANTICS,
                     also provides move constructor and move assignment operators
                     for the vector class.



Overview [Experimental]
-----------------------

There are a few experimental headers which are already part of ncr, but very
incomplete, unstable, and well experimental. Recommendation: Don't use them.
They can be found in the folder `include/ncr/experimental`.

.. code::
    ncr_glutils    - Utilities to work with OpenGL
    ncr_shader     - Shader stuff for OpenGL


License Information
-------------------

The headers are licensed under the MIT License. For more information, see the
LICENSE file.
