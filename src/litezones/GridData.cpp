#include "GridData.h"

#include <algorithm>
#include <map>

namespace
{
    std::vector<int> PrefixSum(const std::vector<int>& values)
    {
        std::vector<int> result;
        result.reserve(values.size() + 1);
        result.push_back(0);
        int sum = 0;
        for (const int value : values)
        {
            sum += value;
            result.push_back(sum);
        }
        return result;
    }

    std::vector<int> AdjacentDifference(const std::vector<int>& values)
    {
        std::vector<int> result;
        if (values.size() <= 1)
        {
            return result;
        }
        result.reserve(values.size() - 1);
        for (size_t i = 0; i + 1 < values.size(); ++i)
        {
            result.push_back(values[i + 1] - values[i]);
        }
        return result;
    }

    // Distinct values in first-appearance order (the input values are always
    // grouped into contiguous segments).
    std::vector<int> Unique(const std::vector<int>& values)
    {
        std::vector<int> result;
        for (const int value : values)
        {
            if (result.empty() || result.back() != value)
            {
                result.push_back(value);
            }
        }
        return result;
    }
}

namespace GridData
{
    Grid::Grid(FancyZonesDataTypes::GridLayoutInfo& model) :
        m_model(&model)
    {
        FromModel();
    }

    void Grid::ModelToZones()
    {
        const int rows = m_model->rows();
        const int cols = m_model->columns();
        const auto& cellMap = m_model->cellChildMap();

        int zoneCount = 0;
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                zoneCount = std::max(zoneCount, cellMap[static_cast<size_t>(row)][static_cast<size_t>(col)]);
            }
        }
        zoneCount++;

        if (zoneCount > rows * cols)
        {
            m_zones.clear();
            return;
        }

        std::vector<int> indexCount(static_cast<size_t>(zoneCount), 0);
        std::vector<int> indexRowLow(static_cast<size_t>(zoneCount), std::numeric_limits<int>::max());
        std::vector<int> indexRowHigh(static_cast<size_t>(zoneCount), 0);
        std::vector<int> indexColLow(static_cast<size_t>(zoneCount), std::numeric_limits<int>::max());
        std::vector<int> indexColHigh(static_cast<size_t>(zoneCount), 0);

        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const int index = cellMap[static_cast<size_t>(row)][static_cast<size_t>(col)];
                indexCount[static_cast<size_t>(index)]++;
                indexRowLow[static_cast<size_t>(index)] = std::min(indexRowLow[static_cast<size_t>(index)], row);
                indexColLow[static_cast<size_t>(index)] = std::min(indexColLow[static_cast<size_t>(index)], col);
                indexRowHigh[static_cast<size_t>(index)] = std::max(indexRowHigh[static_cast<size_t>(index)], row);
                indexColHigh[static_cast<size_t>(index)] = std::max(indexColHigh[static_cast<size_t>(index)], col);
            }
        }

        for (int index = 0; index < zoneCount; ++index)
        {
            if (indexCount[static_cast<size_t>(index)] == 0)
            {
                m_zones.clear();
                return;
            }
            const int expectedCells = (indexRowHigh[static_cast<size_t>(index)] - indexRowLow[static_cast<size_t>(index)] + 1) *
                                      (indexColHigh[static_cast<size_t>(index)] - indexColLow[static_cast<size_t>(index)] + 1);
            if (indexCount[static_cast<size_t>(index)] != expectedCells)
            {
                m_zones.clear();
                return;
            }
        }

        if (static_cast<int>(m_model->rowsPercents().size()) != rows ||
            static_cast<int>(m_model->columnsPercents().size()) != cols)
        {
            m_zones.clear();
            return;
        }
        for (const int percent : m_model->rowsPercents())
        {
            if (percent < 1)
            {
                m_zones.clear();
                return;
            }
        }
        for (const int percent : m_model->columnsPercents())
        {
            if (percent < 1)
            {
                m_zones.clear();
                return;
            }
        }

        const std::vector<int> rowPrefixSum = PrefixSum(m_model->rowsPercents());
        const std::vector<int> colPrefixSum = PrefixSum(m_model->columnsPercents());
        if (rowPrefixSum[static_cast<size_t>(rows)] != Multiplier ||
            colPrefixSum[static_cast<size_t>(cols)] != Multiplier)
        {
            m_zones.clear();
            return;
        }

        m_zones.clear();
        m_zones.reserve(static_cast<size_t>(zoneCount));
        for (int index = 0; index < zoneCount; ++index)
        {
            Zone zone{};
            zone.index = index;
            zone.left = colPrefixSum[static_cast<size_t>(indexColLow[static_cast<size_t>(index)])];
            zone.right = colPrefixSum[static_cast<size_t>(indexColHigh[static_cast<size_t>(index)] + 1)];
            zone.top = rowPrefixSum[static_cast<size_t>(indexRowLow[static_cast<size_t>(index)])];
            zone.bottom = rowPrefixSum[static_cast<size_t>(indexRowHigh[static_cast<size_t>(index)] + 1)];
            m_zones.push_back(zone);
        }
    }

    void Grid::ModelToResizers()
    {
        const int rows = m_model->rows();
        const int cols = m_model->columns();
        const auto& grid = m_model->cellChildMap();

        m_resizers.clear();

        // Horizontal resizers: boundaries where consecutive rows differ.
        for (int row = 1; row < rows; ++row)
        {
            for (int startCol = 0; startCol < cols;)
            {
                const auto row0 = grid[static_cast<size_t>(row - 1)];
                const auto row1 = grid[static_cast<size_t>(row)];
                if (row0[static_cast<size_t>(startCol)] != row1[static_cast<size_t>(startCol)])
                {
                    int endCol = startCol;
                    while (endCol + 1 < cols && row0[static_cast<size_t>(endCol + 1)] != row1[static_cast<size_t>(endCol + 1)])
                    {
                        endCol++;
                    }

                    Resizer resizer{};
                    resizer.orientation = Orientation::Horizontal;
                    for (int col = startCol; col <= endCol; ++col)
                    {
                        resizer.negativeSideIndices.push_back(row0[static_cast<size_t>(col)]);
                        resizer.positiveSideIndices.push_back(row1[static_cast<size_t>(col)]);
                    }
                    resizer.negativeSideIndices = Unique(resizer.negativeSideIndices);
                    resizer.positiveSideIndices = Unique(resizer.positiveSideIndices);
                    m_resizers.push_back(resizer);

                    startCol = endCol + 1;
                }
                else
                {
                    startCol++;
                }
            }
        }

        // Vertical resizers: boundaries where consecutive columns differ.
        for (int col = 1; col < cols; ++col)
        {
            for (int startRow = 0; startRow < rows;)
            {
                const auto col0 = grid[static_cast<size_t>(startRow)][static_cast<size_t>(col - 1)];
                const auto col1 = grid[static_cast<size_t>(startRow)][static_cast<size_t>(col)];
                if (col0 != col1)
                {
                    int endRow = startRow;
                    while (endRow + 1 < rows && grid[static_cast<size_t>(endRow + 1)][static_cast<size_t>(col - 1)] != grid[static_cast<size_t>(endRow + 1)][static_cast<size_t>(col)])
                    {
                        endRow++;
                    }

                    Resizer resizer{};
                    resizer.orientation = Orientation::Vertical;
                    for (int row = startRow; row <= endRow; ++row)
                    {
                        resizer.negativeSideIndices.push_back(grid[static_cast<size_t>(row)][static_cast<size_t>(col - 1)]);
                        resizer.positiveSideIndices.push_back(grid[static_cast<size_t>(row)][static_cast<size_t>(col)]);
                    }
                    resizer.negativeSideIndices = Unique(resizer.negativeSideIndices);
                    resizer.positiveSideIndices = Unique(resizer.positiveSideIndices);
                    m_resizers.push_back(resizer);

                    startRow = endRow + 1;
                }
                else
                {
                    startRow++;
                }
            }
        }
    }

    void Grid::FromModel()
    {
        ModelToZones();
        ModelToResizers();
    }

    void Grid::ZonesToModel()
    {
        std::vector<int> xCoords;
        std::vector<int> yCoords;
        for (const Zone& zone : m_zones)
        {
            xCoords.push_back(zone.right);
            xCoords.push_back(zone.left);
            yCoords.push_back(zone.top);
            yCoords.push_back(zone.bottom);
        }
        std::sort(xCoords.begin(), xCoords.end());
        std::sort(yCoords.begin(), yCoords.end());
        xCoords.erase(std::unique(xCoords.begin(), xCoords.end()), xCoords.end());
        yCoords.erase(std::unique(yCoords.begin(), yCoords.end()), yCoords.end());

        const int rows = static_cast<int>(yCoords.size()) - 1;
        const int cols = static_cast<int>(xCoords.size()) - 1;

        m_model->m_rows = rows;
        m_model->m_columns = cols;
        m_model->m_rowsPercents = AdjacentDifference(yCoords);
        m_model->m_columnsPercents = AdjacentDifference(xCoords);

        std::vector<std::vector<int>> cellMap(static_cast<size_t>(rows), std::vector<int>(static_cast<size_t>(cols), 0));
        for (int index = 0; index < static_cast<int>(m_zones.size()); ++index)
        {
            const Zone& zone = m_zones[static_cast<size_t>(index)];
            const auto startRowIt = std::find(yCoords.begin(), yCoords.end(), zone.top);
            const auto endRowIt = std::find(yCoords.begin(), yCoords.end(), zone.bottom);
            const auto startColIt = std::find(xCoords.begin(), xCoords.end(), zone.left);
            const auto endColIt = std::find(xCoords.begin(), xCoords.end(), zone.right);
            const int startRow = static_cast<int>(startRowIt - yCoords.begin());
            const int endRow = static_cast<int>(endRowIt - yCoords.begin());
            const int startCol = static_cast<int>(startColIt - xCoords.begin());
            const int endCol = static_cast<int>(endColIt - xCoords.begin());

            for (int row = startRow; row < endRow; ++row)
            {
                for (int col = startCol; col < endCol; ++col)
                {
                    cellMap[static_cast<size_t>(row)][static_cast<size_t>(col)] = index;
                }
            }
        }
        m_model->m_cellChildMap = std::move(cellMap);
    }

    std::vector<Boundary> Grid::BoundarySegments() const
    {
        std::vector<Boundary> segments;
        const int rows = m_model->rows();
        const int cols = m_model->columns();
        if (rows == 0 || cols == 0)
        {
            return segments;
        }
        const auto& cellMap = m_model->cellChildMap();
        const std::vector<int> rowPrefix = PrefixSum(m_model->rowsPercents());
        const std::vector<int> colPrefix = PrefixSum(m_model->columnsPercents());

        for (int row = 0; row + 1 < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                if (cellMap[static_cast<size_t>(row)][static_cast<size_t>(col)] !=
                    cellMap[static_cast<size_t>(row + 1)][static_cast<size_t>(col)])
                {
                    Boundary segment{};
                    segment.left = colPrefix[static_cast<size_t>(col)];
                    segment.right = colPrefix[static_cast<size_t>(col + 1)];
                    segment.top = rowPrefix[static_cast<size_t>(row + 1)];
                    segment.bottom = rowPrefix[static_cast<size_t>(row + 1)];
                    segments.push_back(segment);
                }
            }
        }
        for (int col = 0; col + 1 < cols; ++col)
        {
            for (int row = 0; row < rows; ++row)
            {
                if (cellMap[static_cast<size_t>(row)][static_cast<size_t>(col)] !=
                    cellMap[static_cast<size_t>(row)][static_cast<size_t>(col + 1)])
                {
                    Boundary segment{};
                    segment.left = colPrefix[static_cast<size_t>(col + 1)];
                    segment.right = colPrefix[static_cast<size_t>(col + 1)];
                    segment.top = rowPrefix[static_cast<size_t>(row)];
                    segment.bottom = rowPrefix[static_cast<size_t>(row + 1)];
                    segments.push_back(segment);
                }
            }
        }
        return segments;
    }

    int Grid::ResizerPosition(int resizerIndex) const
    {
        if (resizerIndex < 0 || resizerIndex >= static_cast<int>(m_resizers.size()))
        {
            return -1;
        }
        const Resizer& resizer = m_resizers[static_cast<size_t>(resizerIndex)];
        if (resizer.positiveSideIndices.empty())
        {
            return -1;
        }
        const Zone& zone = m_zones[static_cast<size_t>(resizer.positiveSideIndices.front())];
        return (resizer.orientation == Orientation::Horizontal) ? zone.top : zone.left;
    }

    bool Grid::CanSplit(int zoneIndex, int position, Orientation orientation) const
    {
        if (zoneIndex < 0 || zoneIndex >= static_cast<int>(m_zones.size()))
        {
            return false;
        }
        const Zone& zone = m_zones[static_cast<size_t>(zoneIndex)];
        if (orientation == Orientation::Horizontal)
        {
            return zone.top + m_minZoneHeight <= position && position <= zone.bottom - m_minZoneHeight;
        }
        return zone.left + m_minZoneWidth <= position && position <= zone.right - m_minZoneWidth;
    }

    void Grid::Split(int zoneIndex, int position, Orientation orientation)
    {
        if (!CanSplit(zoneIndex, position, orientation))
        {
            return;
        }

        Zone zone1 = m_zones[static_cast<size_t>(zoneIndex)];
        Zone zone2 = zone1;

        m_zones.erase(m_zones.begin() + zoneIndex);
        if (orientation == Orientation::Horizontal)
        {
            zone1.bottom = position;
            zone2.top = position;
        }
        else
        {
            zone1.right = position;
            zone2.left = position;
        }

        m_zones.insert(m_zones.begin() + zoneIndex, zone1);
        m_zones.insert(m_zones.begin() + zoneIndex + 1, zone2);

        ZonesToModel();
        FromModel();
    }

    void Grid::Split2x2(int zoneIndex)
    {
        if (zoneIndex < 0 || zoneIndex >= static_cast<int>(m_zones.size()))
        {
            return;
        }
        const Zone zone = m_zones[static_cast<size_t>(zoneIndex)];
        const int midY = (zone.top + zone.bottom) / 2;
        const int midX = (zone.left + zone.right) / 2;

        const auto findZone = [this](int top, int bottom, int left, int right) -> int {
            for (size_t i = 0; i < m_zones.size(); ++i)
            {
                if (m_zones[i].top == top && m_zones[i].bottom == bottom &&
                    m_zones[i].left == left && m_zones[i].right == right)
                {
                    return static_cast<int>(i);
                }
            }
            return -1;
        };

        if (CanSplit(zoneIndex, midY, Orientation::Horizontal))
        {
            Split(zoneIndex, midY, Orientation::Horizontal);
            const int topIndex = findZone(zone.top, midY, zone.left, zone.right);
            if (topIndex >= 0)
            {
                Split(topIndex, midX, Orientation::Vertical);
            }
            const int bottomIndex = findZone(midY, zone.bottom, zone.left, zone.right);
            if (bottomIndex >= 0)
            {
                Split(bottomIndex, midX, Orientation::Vertical);
            }
        }
        else if (CanSplit(zoneIndex, midX, Orientation::Vertical))
        {
            Split(zoneIndex, midX, Orientation::Vertical);
        }
    }

    std::pair<std::vector<int>, Zone> Grid::ComputeClosure(const std::vector<int>& indices) const
    {
        Zone closureZone{};
        closureZone.index = -1;
        closureZone.left = std::numeric_limits<int>::max();
        closureZone.right = std::numeric_limits<int>::min();
        closureZone.top = std::numeric_limits<int>::max();
        closureZone.bottom = std::numeric_limits<int>::min();

        if (indices.empty())
        {
            return { {}, closureZone };
        }

        const auto extend = [&closureZone](const Zone& zone) {
            closureZone.left = std::min(closureZone.left, zone.left);
            closureZone.right = std::max(closureZone.right, zone.right);
            closureZone.top = std::min(closureZone.top, zone.top);
            closureZone.bottom = std::max(closureZone.bottom, zone.bottom);
        };

        for (const int index : indices)
        {
            if (index >= 0 && index < static_cast<int>(m_zones.size()))
            {
                extend(m_zones[static_cast<size_t>(index)]);
            }
        }

        bool possiblyBroken = true;
        while (possiblyBroken)
        {
            possiblyBroken = false;
            for (const Zone& zone : m_zones)
            {
                const int area = (zone.bottom - zone.top) * (zone.right - zone.left);

                const int cutLeft = std::max(closureZone.left, zone.left);
                const int cutRight = std::min(closureZone.right, zone.right);
                const int cutTop = std::max(closureZone.top, zone.top);
                const int cutBottom = std::min(closureZone.bottom, zone.bottom);

                const int newArea = std::max(0, cutBottom - cutTop) * std::max(0, cutRight - cutLeft);
                if (newArea != 0 && newArea != area)
                {
                    extend(zone);
                    possiblyBroken = true;
                }
            }
        }

        std::vector<int> result;
        for (const Zone& zone : m_zones)
        {
            const bool inside = closureZone.left <= zone.left && zone.right <= closureZone.right &&
                                closureZone.top <= zone.top && zone.bottom <= closureZone.bottom;
            if (inside)
            {
                result.push_back(zone.index);
            }
        }
        return { result, closureZone };
    }

    std::vector<int> Grid::MergeClosureIndices(const std::vector<int>& indices) const
    {
        return ComputeClosure(indices).first;
    }

    void Grid::DoMerge(const std::vector<int>& indices)
    {
        if (indices.empty())
        {
            return;
        }

        int lowestIndex = indices.front();
        for (const int index : indices)
        {
            lowestIndex = std::min(lowestIndex, index);
        }

        const auto [closureIndices, closureZone] = ComputeClosure(indices);

        std::vector<Zone> remaining;
        for (const Zone& zone : m_zones)
        {
            const bool inClosure = std::find(closureIndices.begin(), closureIndices.end(), zone.index) != closureIndices.end();
            if (!inClosure)
            {
                remaining.push_back(zone);
            }
        }

        m_zones = std::move(remaining);
        m_zones.insert(m_zones.begin() + lowestIndex, closureZone);

        ZonesToModel();
        FromModel();
    }

    bool Grid::CanDrag(int resizerIndex, int delta) const
    {
        if (resizerIndex < 0 || resizerIndex >= static_cast<int>(m_resizers.size()))
        {
            return false;
        }
        const Resizer& resizer = m_resizers[static_cast<size_t>(resizerIndex)];
        const int minZoneSize = (resizer.orientation == Orientation::Vertical) ? m_minZoneWidth : m_minZoneHeight;

        const auto getSize = [this, &resizer](int zoneIndex) {
            const Zone& zone = m_zones[static_cast<size_t>(zoneIndex)];
            return (resizer.orientation == Orientation::Vertical) ? zone.right - zone.left : zone.bottom - zone.top;
        };

        for (const int zoneIndex : resizer.positiveSideIndices)
        {
            if (getSize(zoneIndex) - delta < minZoneSize)
            {
                return false;
            }
        }
        for (const int zoneIndex : resizer.negativeSideIndices)
        {
            if (getSize(zoneIndex) + delta < minZoneSize)
            {
                return false;
            }
        }
        return true;
    }

    void Grid::Drag(int resizerIndex, int delta)
    {
        if (!CanDrag(resizerIndex, delta))
        {
            return;
        }

        const Resizer& resizer = m_resizers[static_cast<size_t>(resizerIndex)];
        for (const int zoneIndex : resizer.positiveSideIndices)
        {
            Zone& zone = m_zones[static_cast<size_t>(zoneIndex)];
            if (resizer.orientation == Orientation::Horizontal)
            {
                zone.top += delta;
            }
            else
            {
                zone.left += delta;
            }
        }
        for (const int zoneIndex : resizer.negativeSideIndices)
        {
            Zone& zone = m_zones[static_cast<size_t>(zoneIndex)];
            if (resizer.orientation == Orientation::Horizontal)
            {
                zone.bottom += delta;
            }
            else
            {
                zone.right += delta;
            }
        }

        ZonesToModel();
        FromModel();
    }
}
