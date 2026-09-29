#include "../library/vertex_quad_tree.h"

namespace CGAL {
namespace internal {
/// Common base class for algorithms that work on straight-edge geometries and have a graph as underlying subdivision representation.
/// @tparam K 2D linear kernel.
/// @tparam Derived the deriving class.
/// @tparam Graph the underlying graph.
template<class K, class Derived, class GraphSimplificationAlgorithm>
class Straight_simplification_algorithm {
private:
	static const int m_qtDepth = 10;
	const int m_prec = 0;

public:
	template <class Geometry>
	struct Handle {
	  private:
		size_t index;
		explicit Handle(size_t index) : index(index) {}

		friend class Straight_simplification_algorithm<K, Derived, GraphSimplificationAlgorithm>;
	};

private:
	// Graph should be decomposed into paths.
	using Graph = typename GraphSimplificationAlgorithm::Graph;
	using Vertex_handle = typename Graph::Vertex_handle;
	using Edge_handle = typename Graph::Edge_handle;
	using Path_handle = typename Graph::Path_handle;

	std::unique_ptr<GraphSimplificationAlgorithm> m_alg = nullptr;
	Graph m_graph;
	std::vector<std::vector<Edge_handle>> m_inserted_to_edges;
	std::vector<std::vector<Path_handle>> m_inserted_to_paths;
	size_t handleIndex = 0;
	cartocrow::simplification::VertexQuadTree<Graph> m_vqt;
	
	bool initialized = false;

public:
	Straight_simplification_algorithm(const CGAL::Iso_rectangle_2<K>& bbox, double prec = 0) : m_vqt(bbox, m_qtDepth), m_prec(prec) {};

	void computePaths() {
		std::vector<bool> encounteredPath(m_graph.number_of_paths(), false);
		for (auto ehs : m_inserted_to_edges) {
			encounteredPath.clear();
			std::vector<Path_handle>& paths = m_inserted_to_paths.emplace_back();
			for (auto eh : ehs) {
				auto p = eh->path();
				auto pi = p->graph_index();
				if (!encounteredPath[pi]) { // first time we encounter this path?
					paths.emplace_back(p); // add it to the vector
					encounteredPath[pi] = true;
				}
			}
		}
	}
	
public:
	/// Simplify while the number of edges exceeds complexity.
	void simplify(size_t targetComplexity) {
		if (!initialized) {
			m_alg = std::make_unique<GraphSimplificationAlgorithm>(m_graph, m_vqt);
			m_graph.initialize();
			m_alg->initialize(false);
			computePaths();
		}
		auto& hist = m_graph.history();

		if (targetComplexity > m_graph.number_of_edges()) {
			// revert
			while (hist.can_undo() && m_graph.number_of_edges() < targetComplexity) {
				hist.undo();
			}
		}
		else if (targetComplexity < m_graph.number_of_edges()) {
			while (hist.can_redo() && m_graph.number_of_edges() > targetComplexity) {
				hist.redo();
			}

			if (!hist.can_redo() && targetComplexity < m_graph.number_of_edges()) {
				// already at present, run algorithm further
				m_alg->run([&](int complexity, typename K::FT cost) {
					return complexity <= targetComplexity;
				});
			}
		}
	}

	///// Simplify while the cheapest operation costs less than the given value.
	void simplify(typename K::FT& targetCost) {
		//[&](int complexity, typename K::FT cost) {
		//	return cost <= targetCost;
		//});
	}

	/// Add a polygon to the subdivision.
	template<class Container>
	Handle<CGAL::Polygon_2<K, Container>> add(const CGAL::Polygon_2<K, Container>& polygon) {
		std::vector<Edge_handle>& ehs = m_inserted_to_edges.emplace_back();

		Vertex_handle prev = nullptr;
		Vertex_handle first = nullptr;
		for (auto point : polygon.vertices()) {
			Vertex_handle next = m_vqt.findElement(point, m_prec);
			if (next == nullptr) {
				next = m_graph.add_vertex(point);
				m_vqt.insert(next);
			}
			if (first == nullptr) {
				first = next;
			}
			if (prev != nullptr && !next->is_neighbor_of(prev) && next != prev) {
				ehs.push_back(m_graph.add_edge(prev, next));
			}
			prev = next;
		}

		if (first != prev && !first->is_neighbor_of(prev)) {
			ehs.push_back(m_graph.add_edge(prev, first));
		}
		
		return Handle<CGAL::Polygon_2<K, Container>>{ handleIndex++ };
	}

	template<class Container>
	CGAL::Polygon_2<K, Container> get_simplified(const Handle<CGAL::Polygon_2<K, Container>>& handle) {
		CGAL::Polygon_2<K, Container> polygon;
		for (auto& path : m_inserted_to_paths[handle.index]) {
			Edge_handle eh = path->start();
			while (eh != path->end()) {
				polygon.push_back(eh->source()->point());
				eh = eh->next();
			}
			polygon.push_back(path->end()->source()->point());
			if (!path->cyclic()) {
				polygon.push_back(path->end()->target()->point());
			}
		}
		return polygon;
	}

	///// Add a polygon with holes to the subdivision.
	//template<class Container>
	//void add(const Polygon_with_holes_2<K, Container>& polygon);

	///// Add a multipolygon to the subdivision.
	//template<class Container>
	//void add(Multipolygon_with_holes_2<K, Container>& polygon);

	///// Add a polyline to the subdivision.
	//template<class PointContainer>
	//void add(PointContainer& polyline);

	///// Get the simplified version of the given polygon.
	//template<class Container>
	//Polygon_2<K, Container> get_simplified(Polygon_2<K, Container>& p) {
	//	void* key = static_cast<void*>(&polygon);
	//	return convert_boundaries_to_polygon(m_alg.graph(), m_inserted_to_paths[key]);
	//}

	///// Get the simplified version of the given polygon with holes.
	//template<class Container>
	//Polygon_with_holes_2<K, Container> get_simplified(Polygon_with_holes_2<K, Container>& p);

	///// Get the simplified version of the given polygon with holes.
	//template<class Container>
	//Multipolygon_with_holes_2<K, Container> get_simplified(Multipolygon_with_holes_2<K, Container>& p);

	///// Get the simplified version of the given polyline.
	//template<class PointContainer>
	//PointContainer get_simplified(PointContainer& polyline);

	///// Updates all the inserted geometries to the simplified versions.
	//void update_inserted_geometries();

	///// Simplify a polygon in place.
	//template<class Container>
	//static void simplify_in_place(Polygon<K, Container>& p, size_t c) {
	//	Derived alg;
	//	alg.add(p);
	//	alg.simplify(c);
	//	alg.update_inserted_geometries();		
	//}
	//
	///// Returns a simplified polygon.
	//template<class Container>
	//static Polygon simplify(Polygon<K, Container>& p, size_t c) {
	//	Derived alg;
	//	alg.add(p);
	//	alg.simplify(c);
	//	return alg.get_result(p);		
	//}

	/// And the analogous static functions for the complexity as threshold and the other supported polygonal geometries.
};
}
}