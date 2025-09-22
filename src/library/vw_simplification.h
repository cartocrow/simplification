#pragma once

#include "vertex_removal.h"

namespace cartocrow::simplification {

template <typename G> struct VisvalingamWhyattTraits {
	using Kernel = G::Kernel;

	static Number<Kernel> getCost(typename G::Vertex* v) {
		return CGAL::abs(
		    CGAL::area(v->getPoint(), v->neighbor(0)->getPoint(), v->neighbor(1)->getPoint()));
	}
};

template <typename G> 
using VisvalingamWhyatt = VertexRemoval<G, VisvalingamWhyattTraits<G>>;

} // namespace cartocrow::simplification