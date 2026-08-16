#pragma once

#include "LayoutTypes.h"

#include <utility>
#include <vector>

// Grid editor model, ported from FancyZones' GridData.cs. Operates on a
// GridLayoutInfo in "Multiplier" space (percents 0..10000): zones and resizers
// are derived from the model, mutated on split/merge/drag, and written back as
// new rows/columns percents + cell map. The model's name, spacing and
// sensitivity settings are preserved.
namespace GridData
{
    constexpr int Multiplier = 10000;

    enum class Orientation
    {
        Horizontal,
        Vertical
    };

    struct Zone
    {
        int index = 0;
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    struct Resizer
    {
        Orientation orientation = Orientation::Horizontal;
        std::vector<int> negativeSideIndices; // zones to the left / above
        std::vector<int> positiveSideIndices; // zones to the right / below
    };

    // A cell-boundary segment in Multiplier space (a zero-thickness line used
    // for rendering; left==right for vertical segments, top==bottom for
    // horizontal ones).
    struct Boundary
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
    };

    class Grid
    {
    public:
        explicit Grid(FancyZonesDataTypes::GridLayoutInfo& model);

        const std::vector<Zone>& Zones() const { return m_zones; }
        const std::vector<Resizer>& Resizers() const { return m_resizers; }

        // All visible grid-line segments, used for rendering.
        std::vector<Boundary> BoundarySegments() const;

        int MinZoneWidth() const { return m_minZoneWidth; }
        int MinZoneHeight() const { return m_minZoneHeight; }

        // Position of a resizer in Multiplier space (-1 when invalid).
        int ResizerPosition(int resizerIndex) const;

        // Direct access to the underlying model (for snapshot/restore).
        FancyZonesDataTypes::GridLayoutInfo& Model() { return *m_model; }
        const FancyZonesDataTypes::GridLayoutInfo& Model() const { return *m_model; }

        // Rebuild zones/resizers from the model after an external restore.
        void Reset() { FromModel(); }

        bool CanSplit(int zoneIndex, int position, Orientation orientation) const;
        void Split(int zoneIndex, int position, Orientation orientation);
        // Splits a zone into a 2x2 group; each direction that cannot split is skipped.
        void Split2x2(int zoneIndex);

        // Smallest set of zone indices whose bounding rectangle covers the given set.
        std::vector<int> MergeClosureIndices(const std::vector<int>& indices) const;

        // Merges the closure of the given zone indices into a single zone.
        void DoMerge(const std::vector<int>& indices);

        bool CanDrag(int resizerIndex, int delta) const;
        void Drag(int resizerIndex, int delta);

    private:
        void ModelToZones();
        void ModelToResizers();
        void FromModel();
        void ZonesToModel();

        // Bounding rectangle covering all given zones, extended so it never cuts
        // through another zone; returns the closure's zone indices and rectangle.
        std::pair<std::vector<int>, Zone> ComputeClosure(const std::vector<int>& indices) const;

        FancyZonesDataTypes::GridLayoutInfo* m_model = nullptr;
        std::vector<Zone> m_zones;
        std::vector<Resizer> m_resizers;
        int m_minZoneWidth = 1;
        int m_minZoneHeight = 1;
    };
}
