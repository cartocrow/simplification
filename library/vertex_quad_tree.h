#pragma once

#include <cartocrow/data_structures/point_quad_tree.h>

namespace cartocrow::simplification {

	template<class Graph> 
	struct VertexQuadTreeTraits {

		using Element = Graph::Vertex_handle;
		using Kernel = Graph::Kernel;

		static const Point<Kernel>& get_point(Element elt) {
			return elt->point();
		}
	};	

	template<class Graph>
	using VertexQuadTree = cartocrow::data_structures::PointQuadTree<VertexQuadTreeTraits<Graph>>;
}