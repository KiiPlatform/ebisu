// Catch2's main(), apart from the rest of it, which tests/CMakeLists.txt
// builds with CATCH_AMALGAMATED_CUSTOM_MAIN: a suite with a main() of its own
// links Catch without this one.
#include "catch_amalgamated.hpp"

int main(int argc, char* argv[])
{
    return Catch::Session().run(argc, argv);
}
