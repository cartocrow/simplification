#pragma once

#include "../library/vertex_removal.h"
#include "straight_simplification_algorithm.h"
#include <CGAL/Iso_rectangle_2.h>

namespace CGAL {
namespace detail {
template<class Kernel, bool History>
class VW_algorithm {
public:
	using Graph = cartocrow::simplification::VertexRemovalGraph<Kernel, History>;

private:
	const int DEPTH = 10;

	using VW = cartocrow::simplification::VisvalingamWhyatt<Graph>;
	using VertexTree = VW::VertexTree;
	std::unique_ptr<VertexTree> m_vt;
	VW m_vw;

public:
	VW_algorithm(Graph& graph, std::unique_ptr<VertexTree> vt) : m_vt(std::move(vt)), m_vw(graph, *m_vt) {
		m_vw.initialize(false);
	}

	bool run(std::optional<std::function<bool(int, typename Kernel::FT)>> stop = std::nullopt) {
		return m_vw.run(stop);
	}
};
}

/// The vertex-removal simplification algorithm for straight-edge geometries by Visvalingam and Whyatt.
/// The algorithm iteratively removes the degree-2 vertex v such that the triangle defined by v and its two neighbors has smallest area.
/// @tparam History whether to store the simplification operations for efficient backtracking.
/// @tparam Kernel 2D linear kernel.
/// @tparam Data data stored with each geometry, or std::monostate for none.
/// @tparam PolygonContainer the container of the polygon vertices.
template<bool History = false, class Kernel = CGAL::Epick, class Data = std::monostate, class PolygonContainer = std::vector<CGAL::Point_2<Kernel>>>
class Simplification_VW : public internal::Straight_simplification_algorithm<Kernel, detail::VW_algorithm<Kernel, History>, Data, PolygonContainer> {
	// all methods are inherited from the base class
	using Base = internal::Straight_simplification_algorithm<Kernel, detail::VW_algorithm<Kernel, History>, Data, PolygonContainer>;
public:
	using Base::Base;
};

template<class K, class Data, class PolygonContainer>
Simplification_VW(const cartocrow::StraightGeometryCollection<K, Data, PolygonContainer>&)
    -> Simplification_VW<false, K, Data, PolygonContainer>;

template<class K, class Data, class PolygonContainer>
Simplification_VW(const cartocrow::StraightGeometryCollection<K, Data, PolygonContainer>&, const Simplification_options<K>&)
    -> Simplification_VW<false, K, Data, PolygonContainer>;

// ---- Simplifying a single geometry ---- //
template<class K, class PolygonContainer>
CGAL::Polygon_2<K, PolygonContainer> simplify_VW(const CGAL::Polygon_2<K, PolygonContainer>& polygon, size_t complexity, const Simplification_options<K>& options = {}) {
	return internal::simplify_geometry<Simplification_VW<false, K, std::monostate, PolygonContainer>>(polygon, complexity, options);
}

template<class K, class PolygonContainer>
CGAL::Polygon_with_holes_2<K, PolygonContainer> simplify_VW(const CGAL::Polygon_with_holes_2<K, PolygonContainer>& polygon, size_t complexity, const Simplification_options<K>& options = {}) {
	return internal::simplify_geometry<Simplification_VW<false, K, std::monostate, PolygonContainer>>(polygon, complexity, options);
}

template<class K, class PolygonContainer>
CGAL::Multipolygon_with_holes_2<K, PolygonContainer> simplify_VW(const CGAL::Multipolygon_with_holes_2<K, PolygonContainer>& multiPolygon, size_t complexity, const Simplification_options<K>& options = {}) {
	cartocrow::MultipolygonWithHoles<K, PolygonContainer> input;
	for (const auto& polygon : multiPolygon.polygons_with_holes()) {
		input.add_polygon_with_holes(polygon);
	}
	return internal::simplify_geometry<Simplification_VW<false, K, std::monostate, PolygonContainer>>(input, complexity, options);
}

template<class K>
cartocrow::Polyline<K> simplify_VW(const cartocrow::Polyline<K>& polyline, size_t complexity, const Simplification_options<K>& options = {}) {
	return internal::simplify_geometry<Simplification_VW<false, K>>(polyline, complexity, options);
}

template<class K>
cartocrow::MultiPolyline<K> simplify_VW(const cartocrow::MultiPolyline<K>& multiPolyline, size_t complexity, const Simplification_options<K>& options = {}) {
	return internal::simplify_geometry<Simplification_VW<false, K>>(multiPolyline, complexity, options);
}
}
