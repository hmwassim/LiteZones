#pragma once

#include <windows.h>

#include <coguid.h>

#include <string>
#include <vector>

namespace DefaultValues
{
    constexpr int ZoneCount = 3;
    constexpr bool ShowSpacing = true;
    constexpr int Spacing = 16;
    constexpr int SensitivityRadius = 20;
}

namespace FancyZonesDataTypes
{
    enum class ZoneSetLayoutType : int
    {
        Blank,
        Focus,
        Columns,
        Rows,
        Grid,
        PriorityGrid,
        Custom
    };

    enum class CustomLayoutType : int
    {
        Grid = 0,
        Canvas
    };

    struct CanvasLayoutInfo
    {
        int lastWorkAreaWidth{};
        int lastWorkAreaHeight{};

        struct Rect
        {
            int x{};
            int y{};
            int width{};
            int height{};
        };
        std::vector<Rect> zones;
        int sensitivityRadius{};
    };

    struct GridLayoutInfo
    {
        GridLayoutInfo() = default;
        GridLayoutInfo(int rows, int columns);

        std::vector<int>& rowsPercents() { return m_rowsPercents; }
        std::vector<int>& columnsPercents() { return m_columnsPercents; }
        std::vector<std::vector<int>>& cellChildMap() { return m_cellChildMap; }

        int rows() const { return m_rows; }
        int columns() const { return m_columns; }
        const std::vector<int>& rowsPercents() const { return m_rowsPercents; }
        const std::vector<int>& columnsPercents() const { return m_columnsPercents; }
        const std::vector<std::vector<int>>& cellChildMap() const { return m_cellChildMap; }

        bool showSpacing() const { return m_showSpacing; }
        int spacing() const { return m_spacing; }
        int sensitivityRadius() const { return m_sensitivityRadius; }

        int zoneCount() const;

        int m_rows{};
        int m_columns{};
        std::vector<int> m_rowsPercents;
        std::vector<int> m_columnsPercents;
        std::vector<std::vector<int>> m_cellChildMap;
        bool m_showSpacing{};
        int m_spacing{};
        int m_sensitivityRadius{};
    };

    struct CustomLayoutData
    {
        std::wstring name;
        CustomLayoutType type{};
        CanvasLayoutInfo canvas;
        GridLayoutInfo grid;
    };
}

struct LayoutData
{
    GUID uuid = GUID_NULL;
    FancyZonesDataTypes::ZoneSetLayoutType type = FancyZonesDataTypes::ZoneSetLayoutType::PriorityGrid;
    bool showSpacing = DefaultValues::ShowSpacing;
    int spacing = DefaultValues::Spacing;
    int zoneCount = DefaultValues::ZoneCount;
    int sensitivityRadius = DefaultValues::SensitivityRadius;
};

inline bool operator==(const LayoutData& lhs, const LayoutData& rhs)
{
    return lhs.uuid == rhs.uuid &&
           lhs.type == rhs.type &&
           lhs.showSpacing == rhs.showSpacing &&
           lhs.spacing == rhs.spacing &&
           lhs.zoneCount == rhs.zoneCount &&
           lhs.sensitivityRadius == rhs.sensitivityRadius;
}
