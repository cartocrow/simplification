#pragma once

#include "vertex_removal.h"

namespace cartocrow::simplification {

template <typename K> struct VisvalingamWhyattTraits {
	using Kernel = K;

	static Number<K> getCost(VRVertex<K>* v) {
		return CGAL::abs(
		    CGAL::area(v->getPoint(), v->neighbor(0)->getPoint(), v->neighbor(1)->getPoint()));
	}
};

template <typename K>
using VisvalingamWhyatt = VertexRemoval<ObliviousGraph<VRGraph<K>>, VisvalingamWhyattTraits<K>>;

} // namespace cartocrow::simplification