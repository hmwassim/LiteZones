// Zone/Layout geometry tests
// (Zone.Spec.cpp, Layout.Spec.cpp), plus a tiny assert harness. No third-party
// test framework. Exit code 0 = all passed.

#include "TestHarness.h"

#include <iostream>

// Test entry points (defined in separate files).
void RunZoneMathTests();
void RunLayoutTests();
void RunEditorTests();
void RunStoreTests();
void RunJsonTests();
void RunUtilsTests();
void RunColorsTests();

int main()
{
    RunZoneMathTests();
    RunLayoutTests();
    RunEditorTests();
    RunStoreTests();
    RunJsonTests();
    RunUtilsTests();
    RunColorsTests();

    if (g_failures == 0)
    {
        std::cout << "ZoneTests: all checks passed\n";
        return 0;
    }
    std::cerr << "ZoneTests: " << g_failures << " check(s) FAILED\n";
    return 1;
}
