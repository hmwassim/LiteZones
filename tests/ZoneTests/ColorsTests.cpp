#include "TestHarness.h"

#include "../../src/litezones/Colors.h"
#include "../../src/litezones/Settings.h"

void TestHexToRgbViaGetZoneColors()
{
    SettingsData settings{};
    settings.zoneColor = L"#FF0000";
    settings.zoneBorderColor = L"#00FF00";
    settings.zoneHighlightColor = L"#0000FF";
    settings.zoneNumberColor = L"#FFFFFF";
    settings.highlightOpacity = 75;

    const auto colors = Colors::GetZoneColors(settings);
    CHECK(colors.primaryColor == RGB(255, 0, 0));
    CHECK(colors.borderColor == RGB(0, 255, 0));
    CHECK(colors.highlightColor == RGB(0, 0, 255));
    CHECK(colors.numberColor == RGB(255, 255, 255));
    CHECK(colors.highlightOpacity == 75);
}

void TestHexToRgbCaseInsensitive()
{
    SettingsData settings{};
    settings.zoneColor = L"#aAbBcC";
    settings.zoneBorderColor = L"#000000";
    settings.zoneHighlightColor = L"#000000";
    settings.zoneNumberColor = L"#000000";

    const auto colors = Colors::GetZoneColors(settings);
    CHECK(colors.primaryColor == RGB(0xAA, 0xBB, 0xCC));
}

void TestHexToRgbInvalidInput()
{
    SettingsData settings{};
    settings.zoneColor = L"not-a-color";
    settings.zoneBorderColor = L"#";
    settings.zoneHighlightColor = L"#GGHHII";
    settings.zoneNumberColor = L"";
    settings.highlightOpacity = 50;

    const auto colors = Colors::GetZoneColors(settings);
    CHECK(colors.primaryColor == 0);
    CHECK(colors.borderColor == 0);
    CHECK(colors.highlightColor == 0);
    CHECK(colors.numberColor == 0);
}

void RunColorsTests()
{
    TestHexToRgbViaGetZoneColors();
    TestHexToRgbCaseInsensitive();
    TestHexToRgbInvalidInput();
}
