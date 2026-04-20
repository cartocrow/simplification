#pragma once

#include <cartocrow/data_structures/point_quad_tree.h>

namespace cartocrow::simplification {

	template<class Graph> 
	struct VertexQuadTreeTraits {

		using Element = Graph::Vertex;
		using Kernel = Graph::Kernel;

		static Point<Kernel>& get_point(Element& elt) {
			return elt.getPoint();
		}
	};

	template<class Graph>
	using VertexQuadTree = cartocrow::data_structures::PointQuadTree<VertexQuadTreeTraits<Graph>>;
}