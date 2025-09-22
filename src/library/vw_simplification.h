#pragma once

#include "vertex_removal.h"

namespace cartocrow::simplification {

/// <summary>
/// Traits for running VisvalingamWhyatt vertex-removal algorithms. The cost is equal to the area of the spanned triangle.
/// </summary>
/// <typeparam name="G">The graph type for the algorithm</typeparam>
template <typename G> struct VisvalingamWhyattTraits {
	using Kernel = G::Kernel;

	static Number<Kernel> getCost(typename G::Vertex* v) {
		return CGAL::abs(
		    CGAL::area(v->getPoint(), v->neighbor(0)->getPoint(), v->neighbor(1)->getPoint()));
	}
};

/// <summary>
/// Shorthand for the VisvalingamWhyatt vertex-removal algorithm.
/// </summary>
/// <typeparam name="G">The graph type for the algorithm</typeparam>
template <typename G> 
using VisvalingamWhyatt = VertexRemoval<G, VisvalingamWhyattTraits<G>>;

} // namespace cartocrow::simplification