#include "../library/vertex_quad_tree.h"

#include <cartocrow/core/straight_geometry_collection.h>

namespace CGAL {
namespace internal {
/// Common base class for algorithms that work on straight-edge geometries and have a graph as underlying subdivision representation.
/// @tparam K 2D linear kernel.
/// @tparam Derived the deriving class.
/// @tparam Graph the underlying graph.
template<class K, class Derived, class GraphSimplificationAlgorithm, class Data = std::monostate, class PolygonContainer = std::vector<CGAL::Point_2<K>>>
class Straight_simplification_algorithm {
private:
	static const int m_qtDepth = 10;
	const int m_prec = 0;
	using Point = K::Point_2;

	// GraphTraits of the Graph should have decomposed set to true.
	using Graph = typename GraphSimplificationAlgorithm::Graph;
	using Vertex_handle = typename Graph::Vertex_handle;
	using Edge_handle = typename Graph::Edge_handle;
	using Path_handle = typename Graph::Path_handle;

	template <class Element>
	struct Topology {
		virtual ~Topology() = default;
	};

	template <class Element>
	struct PointTopology : public Topology<Element> {
		Point point;
	};
	
	template <class Element>
	struct MultiPointTopology : public Topology<Element> {
		std::vector<Point> points;
	};

	template <class Element>
	struct PolylineTopology : public Topology<Element> {
		std::vector<Element> elements;
	};

	template <class Element>
	struct MultiPolylineTopology : public Topology<Element> {
		std::vector<std::vector<Element>> elements;
	};

	template <class Element>
	struct PolygonTopology : public Topology<Element> {
		std::vector<Element> elements;
	};

	template <class Element>
	struct PolygonWithHolesTopology : public Topology<Element> {
		std::vector<std::vector<Element>> elements;
	};

	template <class Element>
	struct MultiPolygonWithHolesTopology : public Topology<Element> {
		std::vector<std::vector<std::vector<Element>>> elements;
	};

	struct DirectedPath {
		Path_handle path_handle;
		bool reversed;

		Edge_handle first_edge() const {
			return reversed ? path_handle->last_edge() : path_handle->first_edge();
		}

		Edge_handle last_edge() const {
			return reversed ? path_handle->first_edge() : path_handle->last_edge();
		}
	};

	using VertexTree = cartocrow::simplification::VertexQuadTree<Graph>;
	std::unique_ptr<VertexTree> m_vqt;
	std::unique_ptr<GraphSimplificationAlgorithm> m_alg;
	Graph m_graph;
	std::vector<std::shared_ptr<Topology<Edge_handle>>> m_inserted_to_edges;
	std::vector<std::shared_ptr<Topology<DirectedPath>>> m_inserted_to_paths;
	std::vector<Data> m_data;

private:
	void computePaths() {
		for (auto& topo : m_inserted_to_edges) {
			if (auto* pointT = dynamic_cast<PointTopology<Edge_handle>*>(topo.get())) {
				auto pt = std::make_shared<PointTopology<DirectedPath>>();
				pt->point = pointT->point;
				m_inserted_to_paths.push_back(pt);
			} else if (auto* multiPointT = dynamic_cast<MultiPointTopology<Edge_handle>*>(topo.get())) {
				auto mp = std::make_shared<MultiPointTopology<DirectedPath>>();
				mp->points = multiPointT->points;
				m_inserted_to_paths.push_back(mp);
			} else if (auto* polygonT = dynamic_cast<PolygonTopology<Edge_handle>*>(topo.get())) {
				std::vector<bool> handledPath(m_graph.number_of_paths(), false);
				auto paths = std::make_shared<PolygonTopology<DirectedPath>>();
				auto& ehs = polygonT->elements;
				for (size_t i = 0; i < ehs.size(); ++i) {
					auto eh = ehs[i];
					auto p = eh->path();
					auto pi = p->graph_index();

					// At the end of the path: either the next edge is on a different path, or this is the last edge and we haven't handled this path before.
					bool endOfPath = !handledPath[pi] && (i + 1 == ehs.size() || ehs[i + 1]->path() != p);
					
					if (endOfPath) {
						assert(p->first_edge() == eh || p->last_edge() == eh);
						bool reversed = eh == p->first_edge();
						paths->elements.emplace_back(p, reversed);
						handledPath[pi] = true;
					}
				}
				m_inserted_to_paths.push_back(paths);
			}
			else {
				// TODO
				continue;
			}
		}

		m_inserted_to_edges.clear();
	}

	/// Add a polygon to the subdivision.
	void add(const CGAL::Polygon_2<K, PolygonContainer>& polygon) {
		// todo fix
		PolygonTopology<Edge_handle> ehs;

		Vertex_handle prev = nullptr;
		Vertex_handle first = nullptr;
		for (auto point : polygon.vertices()) {
			Vertex_handle next = m_vqt->findElement(point, m_prec);
			if (next == nullptr) {
				next = m_graph.add_vertex(point);
				m_vqt->insert(next);
			}
			if (first == nullptr) {
				first = next;
			}
			if (prev != nullptr && !next->is_neighbor_of(prev) && next != prev) {
				ehs.elements.push_back(m_graph.add_edge(prev, next));
			}
			prev = next;
		}

		if (first != prev && !first->is_neighbor_of(prev)) {
			ehs.elements.push_back(m_graph.add_edge(prev, first));
		}
	
		m_inserted_to_edges.push_back(std::make_shared<PolygonTopology<Edge_handle>>(ehs));
	}


	void add(const CGAL::Polygon_with_holes_2<K, PolygonContainer>& polygon) {
	}

	void add(const CGAL::Multipolygon_with_holes_2<K, PolygonContainer>& multiPolygon) {
	}

	void add(const cartocrow::Polyline<K>& polyline) {
	}

	void add(const cartocrow::MultiPolyline<K>& multiPolyline) {
	}

	void add(const cartocrow::Point<K>& point) {
	}

	void add(const cartocrow::MultiPoint<K>& multiPoint) {
	}

  public:
	Straight_simplification_algorithm(cartocrow::StraightGeometryCollection<K, Data, PolygonContainer> collection, double prec = 0) : m_vqt(std::make_unique<VertexTree>(collection.bbox(), m_qtDepth)), m_prec(prec) {
		// insert geometries into graph
		for (auto& geometry : collection.geometries()) {
			std::visit([this](auto& geom) { add(geom); }, geometry);
		}
		if constexpr (!std::is_same_v<Data, std::monostate>) {
			for (auto& data : collection.data()) {
				m_data.push_back(data);
			}
		}

		// intialize everything and compute the paths
		m_graph.initialize();
		computePaths();
		m_alg = std::make_unique<GraphSimplificationAlgorithm>(m_graph, std::move(m_vqt));
	};

	cartocrow::StraightGeometryCollection<K, Data, PolygonContainer>
	get_collection() {
		cartocrow::StraightGeometryCollection<K, Data, PolygonContainer> collection;
		for (size_t i = 0; i < m_inserted_to_paths.size(); ++i) {
			auto& topo = m_inserted_to_paths[i];
			if (auto* pointT = dynamic_cast<PointTopology<DirectedPath>*>(topo.get())) {
				if constexpr (!std::is_same_v<Data, std::monostate>) {
					collection.insert(pointT->point, m_data[i]);
				}
				else {
					collection.insert(pointT->point);
				}
			}
			else if (auto* multiPointT = dynamic_cast<MultiPointTopology<DirectedPath>*>(topo.get())) {
				if constexpr (!std::is_same_v<Data, std::monostate>) {
					collection.insert(cartocrow::MultiPoint(multiPointT->points), m_data[i]);
				}
				else {
					collection.insert(cartocrow::MultiPoint(multiPointT->points));
				}
			}
			else if (auto* polygonT = dynamic_cast<PolygonTopology<DirectedPath>*>(topo.get())) {
				CGAL::Polygon_2<K, PolygonContainer> polygon;
				for (auto& path : polygonT->elements) {
					Edge_handle eh = path.first_edge();
					while (eh != path.last_edge()) {
						polygon.push_back(eh->source()->point());
						eh = eh->next();
					}
					polygon.push_back(path.last_edge()->source()->point());
					if (!path.path_handle->cyclic()) {
						polygon.push_back(path.last_edge()->target()->point());
					}
				}
				if constexpr (!std::is_same_v<Data, std::monostate>) {
					collection.insert(polygon, m_data[i]);
				}
				else {
					collection.insert(polygon);
				}
			}
			else {
				// TODO
				continue;
			}
		}
		return collection;
	}


	/// Simplify while the number of edges exceeds complexity.
	void simplify(size_t targetComplexity) {
		if constexpr (Graph::Graph_traits::historic) {
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
		else {
			if (targetComplexity < m_graph.number_of_edges()) {
				// already at present, run algorithm further
				m_alg->run([&](int complexity, typename K::FT cost) {
					return complexity <= targetComplexity;
				});
			}
		}
	}

	///// Simplify while the cheapest operation costs less than the given value.
	//void simplify(typename K::FT& targetCost) {
		//[&](int complexity, typename K::FT cost) {
		//	return cost <= targetCost;
		//});
	//}

	/// Returns a simplified polygon.
	static CGAL::Polygon_2<K, PolygonContainer> simplify(const CGAL::Polygon_2<K, PolygonContainer>& p, size_t c) { //requires (std::same_as(Data, std::monostate) && !Graph::Graph_traits::historic) {
		cartocrow::StraightGeometryCollection<K, Data, PolygonContainer> collection;
		auto pHandle = collection.insert(p);
		Derived alg(collection);
		alg.simplify(c);
		auto simplified = alg.get_collection();
		return simplified.get_geometry(pHandle);
	}
};
}
}