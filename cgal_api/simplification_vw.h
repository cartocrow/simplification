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

	VW_algorithm(Graph& graph, const CGAL::Iso_rectangle_2<Kernel>& box) : m_vt(std::make_unique<VertexTree>(box, DEPTH)), m_vw(graph, *m_vt) {
		m_vw.initialize(true); 
	}

	bool run(std::optional<std::function<bool(int, typename Kernel::FT)>> stop = std::nullopt) {
		return m_vw.run(stop);
	}
};
}

/// The vertex-removal simplification algorithm for straight-edge geometries by Visvalingam and Whyatt.
/// The algorithm iteratively removes the degree-2 vertex v such that the triangle defined by v and its two neighbors has smallest area.
/// @tparam Kernel 2D linear kernel.
/// @tparam History whether to store the simplification operations for efficient backtracking.
template<bool History = false, class Kernel = CGAL::Epick, class Data = std::monostate, class PolygonContainer = std::vector<CGAL::Point_2<Kernel>>>
class Simplification_VW : public internal::Straight_simplification_algorithm<Kernel, Simplification_VW<History, Kernel, Data, PolygonContainer>, detail::VW_algorithm<Kernel, History>, Data> {
	// all methods are inherited from the base class
	using Base = internal::Straight_simplification_algorithm<Kernel, Simplification_VW<History, Kernel, Data, PolygonContainer>, detail::VW_algorithm<Kernel, History>, Data>;
public:
	using Base::Base;
};

template<
	class K,
	class Data,
	class PolygonContainer
>
Simplification_VW(
	cartocrow::StraightGeometryCollection<K, Data, PolygonContainer>
) -> Simplification_VW<false, K, Data, PolygonContainer>;
}