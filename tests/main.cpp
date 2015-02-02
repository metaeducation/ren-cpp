#include <iostream>
#include "rencpp/ren.hpp"

#define CATCH_CONFIG_RUNNER
#include "catch.hpp"

int main(int argc, char * argv[]) // char* const conflicts w/Rebol
{
    Catch::Session session; // There must be exactly once instance

    // writing to session.configData() here sets defaults
    // this is the preferred way to set them

    int returnCode = session.applyCommandLine(argc, argv);
    if (returnCode != 0) // Indicates a command line error
        return returnCode;

    // Writing to session.configData() or session.Config() here
    // overrides command line args.
    // Only do this if you know you need to.

    return session.run();
}
