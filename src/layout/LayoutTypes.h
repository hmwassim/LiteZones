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

namespace LiteZonesTypes
{
    enum class ZoneSetLayoutType : int
    {
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

            friend bool operator==(const Rect& lhs, const Rect& rhs)
            {
                return lhs.x == rhs.x && lhs.y == rhs.y &&
                       lhs.width == rhs.width && lhs.height == rhs.height;
            }
        };
        std::vector<Rect> zones;
        int sensitivityRadius{};

        friend bool operator==(const CanvasLayoutInfo& lhs, const CanvasLayoutInfo& rhs)
        {
            return lhs.lastWorkAreaWidth == rhs.lastWorkAreaWidth &&
                   lhs.lastWorkAreaHeight == rhs.lastWorkAreaHeight &&
                   lhs.zones == rhs.zones &&
                   lhs.sensitivityRadius == rhs.sensitivityRadius;
        }
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

        void setRows(int value) { m_rows = value; }
        void setColumns(int value) { m_columns = value; }

        bool showSpacing() const { return m_showSpacing; }
        int spacing() const { return m_spacing; }
        int sensitivityRadius() const { return m_sensitivityRadius; }

        void setShowSpacing(bool value) { m_showSpacing = value; }
        void setSpacing(int value) { m_spacing = value; }
        void setSensitivityRadius(int value) { m_sensitivityRadius = value; }

        int zoneCount() const;

        friend bool operator==(const GridLayoutInfo& lhs, const GridLayoutInfo& rhs)
        {
            return lhs.m_rows == rhs.m_rows &&
                   lhs.m_columns == rhs.m_columns &&
                   lhs.m_rowsPercents == rhs.m_rowsPercents &&
                   lhs.m_columnsPercents == rhs.m_columnsPercents &&
                   lhs.m_cellChildMap == rhs.m_cellChildMap &&
                   lhs.m_showSpacing == rhs.m_showSpacing &&
                   lhs.m_spacing == rhs.m_spacing &&
                   lhs.m_sensitivityRadius == rhs.m_sensitivityRadius;
        }

    private:
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

        friend bool operator==(const CustomLayoutData& lhs, const CustomLayoutData& rhs)
        {
            return lhs.name == rhs.name &&
                   lhs.type == rhs.type &&
                   lhs.canvas == rhs.canvas &&
                   lhs.grid == rhs.grid;
        }
    };
}

struct LayoutData
{
    GUID uuid = GUID_NULL;
    LiteZonesTypes::ZoneSetLayoutType type = LiteZonesTypes::ZoneSetLayoutType::PriorityGrid;
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
