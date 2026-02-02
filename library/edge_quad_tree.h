#pragma once

#include <cartocrow/datastructures/quad_tree.h>

namespace cartocrow::simplification {

	template<class Graph>
	struct EdgeQuadTreeTraits {
		using Element = Graph::Edge;
		using Kernel = Graph::Kernel;

		static Rectangle<Kernel> get_bounding_box(Element& elt) {
			Segment<Kernel> seg = elt.getSegment();
			return utils::boxOf({ seg.start(), seg.end() });
		}

		static bool element_overlaps_rectangle(Element& elt, Rectangle<Kernel>& rect) {
			Segment<Kernel> seg = elt.getSegment();
			return utils::overlaps(rect, seg);
		}
	};

	template<class Graph>
	using EdgeQuadTree = cartocrow::datastructures::QuadTree<EdgeQuadTreeTraits<Graph>>;
}