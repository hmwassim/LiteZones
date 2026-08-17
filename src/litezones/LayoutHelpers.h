#pragma once

#include "LayoutTypes.h"

#include <algorithm>
#include <windows.h>

namespace LayoutHelpers
{
    inline LayoutData MakeDefaultLayout()
    {
        LayoutData layout;
        layout.type = LiteZonesTypes::ZoneSetLayoutType::PriorityGrid;
        layout.zoneCount = DefaultValues::ZoneCount;
        return layout;
    }

    inline LiteZonesTypes::GridLayoutInfo MakeGridLayout(int rows, int cols)
    {
        LiteZonesTypes::GridLayoutInfo grid(rows, cols);
        const int rowPct = 10000 / rows;
        const int colPct = 10000 / cols;
        for (int i = 0; i < rows; ++i)
        {
            grid.rowsPercents()[i] = (i < rows - 1) ? rowPct : 10000 - rowPct * (rows - 1);
        }
        for (int i = 0; i < cols; ++i)
        {
            grid.columnsPercents()[i] = (i < cols - 1) ? colPct : 10000 - colPct * (cols - 1);
        }
        int zoneIndex = 0;
        grid.cellChildMap().resize(rows);
        for (int row = 0; row < rows; ++row)
        {
            grid.cellChildMap()[row].resize(cols);
            for (int col = 0; col < cols; ++col)
            {
                grid.cellChildMap()[row][col] = zoneIndex++;
            }
        }
        grid.setShowSpacing(DefaultValues::ShowSpacing);
        grid.setSpacing(DefaultValues::Spacing);
        grid.setSensitivityRadius(DefaultValues::SensitivityRadius);
        return grid;
    }

    inline void ExtendBoundingRect(RECT& acc, bool& empty, const RECT& r)
    {
        if (empty)
        {
            acc = r;
            empty = false;
        }
        else
        {
            acc.left = std::min(acc.left, r.left);
            acc.top = std::min(acc.top, r.top);
            acc.right = std::max(acc.right, r.right);
            acc.bottom = std::max(acc.bottom, r.bottom);
        }
    }
}
