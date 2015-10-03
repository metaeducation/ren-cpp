This include directory is here because it once housed Ren Garden's inclusion of
Andrzej Krzemienski's `std::experimental::optional`.  That has migrated out to
be a dependency of Ren/C++ proper, but the build script still includes this
empty directory...should Ren Garden pick up any third-party includes (or move
its own includes here)
