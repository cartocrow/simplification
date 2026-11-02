#pragma once

#include "common.h"

namespace cartocrow::simplification::ec {

	template<bool UseSelfEdgeData, class VertexData, class ActualEdgeData, KernelType K, class GraphTraits, typename CustomData>
	struct EdgeData {
		using Kernel = K;
		using Edge_handle = std::conditional<UseSelfEdgeData,
			Graph_2_edge<VertexData, EdgeData, Graph_segment_curve_traits_2<K>, GraphTraits>*,
			Graph_2_edge<VertexData, ActualEdgeData, Graph_segment_curve_traits_2<K>, GraphTraits>*>::type; // needed to avoid co-dependent templates for dedicated graphs

		// collapse specification
		bool erase_both; // special case: both endpoints are to be removed
		Point<Kernel> point; // general case: endpoints merge onto this point
		bool creates_difference; // special case: when the edge is collinear with its neighbors, there are no difference-triangles
		Triangle<Kernel> T1, T2; // the two triangles of difference
		Number<Kernel> cost; // the cost of the collapse

		// algorithm 
		bool blocked_by_degzero = false;
		std::vector<Edge_handle> blocked_by;
		std::vector<Edge_handle> blocking;
		int queue_index = -1;

		CustomData custom;
	};

	template<class DT>
	concept DataTraits =
		KernelType<typename DT::Kernel> &&
		GraphType<typename DT::Graph> &&
		DT::Graph::Graph_traits::oriented &&
		DT::Graph::Graph_traits::decomposed &&
		std::is_default_constructible<typename DT::Edge_data>::value &&
		std::is_same<typename DT::Graph::Edge_handle, typename DT::Edge_data::Edge_handle>::value &&
		requires(DT data, typename DT::Graph graph, typename DT::Graph::Edge_handle e) {

		typename DT::Kernel;
		typename DT::Graph;
		typename DT::Edge_data;
		{ data.initialize(graph) };
		{ data.data(e) } -> std::same_as<typename DT::Edge_data&>;
	};

	template<class ST>
	concept ScoreTraits =
		KernelType<typename ST::Kernel> &&
		GraphType<typename ST::Graph> &&
		ST::Graph::Graph_traits::oriented &&
		ST::Graph::Graph_traits::decomposed &&
		requires(ST score, typename ST::Graph graph, typename ST::Graph::Edge_handle e, typename ST::Edge_data edata) {

		typename ST::Kernel;
		typename ST::Graph;
		typename ST::Edge_data;
		//{ score.initialize(graph, data) }; Moved to general Setup concept, to not make this depend on DataTraits
		{ score.determine_collapse(e, edata) };
	};

	template<class DT, class ST>
	concept Setup =
		DataTraits<DT> && ScoreTraits<ST> &&
		std::is_same<typename ST::Kernel, typename DT::Kernel>::value &&
		std::is_same<typename ST::Graph, typename DT::Graph>::value &&
		std::is_same<typename ST::Edge_data, typename DT::Edge_data>::value &&
		requires(ST score, DT data, typename DT::Graph graph) {

			{ score.initialize(graph, data) };
	};

	template<KernelType K, bool Historic, class CustomData = std::monostate>
	class DataTraits_Dedicated {
	public:
		using Kernel = K;
		using Edge_data = EdgeData<true, std::monostate, std::monostate, Kernel, DecomposedGraph<Historic, std::monostate>, CustomData>;
		using Graph = Straight_graph_2<std::monostate, Edge_data, Kernel, DecomposedGraph<Historic, std::monostate>>;

		static void initialize(Graph& graph) {
			// skip
		}

		static Edge_data& data(typename Graph::Edge_handle e) {
			return e->data();
		}
	};

	template<GraphType G, class CustomData = std::monostate>
	class DataTraits_Generic {
	public:
		using Graph = G;
		using Kernel = Graph::Kernel;
		using Edge_data = EdgeData<false, typename G::Vertex_data, typename G::Edge_data, Kernel, typename G::Graph_traits, CustomData>;

		void initialize(Graph& graph) {
			delete m_edge_data;
			m_edge_data = new Graph_edge_map<Graph, Edge_data>(graph, Edge_data());
		}

		~DataTraits_Generic() {
			delete m_edge_data;
		}

		Edge_data& data(typename Graph::Edge_handle e) {
			return (*m_edge_data)[e];
		}
	private:
		Graph_edge_map<Graph, Edge_data>* m_edge_data = nullptr;
	};

	template <DataTraits DT, ScoreTraits ST>
		requires Setup<DT, ST>
	class EdgeCollapse {
	public:
		using Data_traits = DT;
		using Score_traits = ST;

		using Graph = Data_traits::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Edge_handle = Graph::Edge_handle;
		using Edge_data = Data_traits::Edge_data;
		using Kernel = Graph::Kernel;

	private:
		using VertexTree = VertexQuadTree<Graph>;
		using EdgeTree = EdgeQuadTree<Graph>;
		using Queue = cartocrow::IndexedPriorityQueue<StatefulGraphQueueTraits<Edge_handle, DT>>;

		Graph& graph;
		EdgeTree* sqt = nullptr;
		VertexTree* pqt = nullptr;
		Data_traits data_traits;
		Score_traits score_traits;
		Queue queue = Queue(data_traits);

		void update(Edge_handle e) {
			assert(graph.edge(e->graph_index()) == e);

			Edge_data& edata = data_traits.data(e);

			// clear topology
			for (Edge_handle b : edata.blocked_by) {
				utils::listRemove(e, data_traits.data(b).blocking);
			}
			edata.blocked_by.clear();

			// last condition checks for a triangle
			if (e->source()->degree() != 2 || e->target()->degree() != 2 ||
				e->prev()->source() == e->next()->target()) {
				queue.remove(e);
				return;
			}

			score_traits.determine_collapse(e, edata);

			if (queue.contains(e)) {
				queue.update(e);
			}
			else {
				queue.push(e);
			}
		}

		bool blocks(Edge_handle edge, Edge_handle collapse) {
			Edge_handle prev = collapse->prev();
			Edge_handle next = collapse->next();

			if (edge == collapse || edge == prev || edge == next) {
				// involved in collapse
				return false;
			}

			Vertex_handle prev_v = collapse->prev()->source();
			Vertex_handle next_v = collapse->next()->target();;

			bool source_shared = edge->source() == prev_v || edge->source() == next_v;
			bool target_shared = edge->target() == prev_v || edge->target() == next_v;

			auto test_is = [&](std::optional<std::variant<Point<Kernel>, Segment<Kernel>>> is) {
				if (!is.has_value()) {
					// certainly no intersection
					return false;
				}


				if (Point<Kernel>* pt = std::get_if<Point<Kernel>>(&*is)) {
					// make sure it's not the common point
					if (source_shared && safe_test::same_point(*pt, edge->source()->point())) {
						return false;
					}
					if (target_shared && safe_test::same_point(*pt, edge->target()->point())) {
						return false;
					}

					return true;
				}
				else if constexpr (std::is_same<Inexact, Kernel>::value) {

					// running in inexact mode: may need to account for the intersection being a tiny segment near the start vertex

					Segment<Kernel>* ls = std::get_if<Segment<Kernel>>(&*is);

					if (source_shared
						&& safe_test::same_point(ls->source(), edge->source()->point())
						&& safe_test::same_point(ls->target(), edge->source()->point())) {
						return false;
					}

					if (target_shared
						&& safe_test::same_point(ls->source(), edge->target()->point())
						&& safe_test::same_point(ls->target(), edge->target()->point())) {
						return false;
					}

					// intersection doesnt reflect a shared endpoint
					return true;
				}
				else {
					// running in exact mode: 
					// segments always intersect
					return true;
				}
				};

			Edge_data& cdata = data_traits.data(collapse);
			if (test_is(CGAL::intersection(cdata.T1, edge->curve()))) {
				return true;
			}

			if (test_is(CGAL::intersection(cdata.T2, edge->curve()))) {
				return true;
			}

			return false;
		}

		Edge_handle findNextStep() {
			assert(graph.can_perform_operation());

			while (!queue.empty()) {
				Edge_handle e = queue.peek();

				Edge_data& edata = data_traits.data(e);

				if (edata.creates_difference) {
					// possibly blocked?

					Rectangle<Kernel> rect = utils::boxOf(edata.T1, edata.T2);

					edata.blocked_by_degzero = false;

					pqt->findContained(rect, [&edata](Vertex_handle b) {
						if (!edata.T1.has_on_unbounded_side(b->point()) ||
							!edata.T2.has_on_unbounded_side(b->point())) {
							// blocked, by an unmovable vertex
							edata.blocked_by_degzero = true;
						}
						});

					if (!edata.blocked_by_degzero) {

						sqt->findOverlapped(rect, [this, &e](Edge_handle b) {

							if (blocks(b, e)) {
								data_traits.data(b).blocking.push_back(e);
								data_traits.data(e).blocked_by.push_back(b);
							}
							});
					}

				} // else: no difference, cannot be blocked

				if (!edata.blocked_by_degzero && edata.blocked_by.empty()) {
					// not blocked, this is the next step
					return e;
				}
				else {
					// remove the element from the queue as it's not valid and continue searching
					queue.pop();
				}
			}

			// no steps exist
			return nullptr;
		}

		void performStep(Edge_handle e) {
			assert(graph.can_perform_operation());
			assert(queue.peek() == e);

			queue.pop();

			auto& edata = data_traits.data(e);

			// remove from blocking lists and search structure
			Edge_handle prev = e->prev();
			Edge_handle next = e->next();
			sqt->remove(e);
			sqt->remove(prev);
			sqt->remove(next);

			queue.remove(prev);
			queue.remove(next);

			// clear blocking data from collapsing edge.
			// NB: this edge will be removed in the collapse, so we dont need to consider blocked_by
			for (Edge_handle b : edata.blocking) {
				Edge_data& bdata = data_traits.data(b);
				if (utils::listRemove(e, bdata.blocked_by)) {
					if (bdata.blocked_by.empty() && !bdata.blocked_by_degzero) {
						queue.push(b);
					}
				}
			}
			edata.blocking.clear();

			// clear blocking data from previous edge
			{
				Edge_data& pdata = data_traits.data(prev);
				for (Edge_handle b : pdata.blocking) {
					if (b == next) continue; // NB: next could be blocking prev, but will be reset or removed
					auto& bdata = data_traits.data(b);
					if (utils::listRemove(prev, bdata.blocked_by)) {
						if (bdata.blocked_by.empty() && !bdata.blocked_by_degzero) {
							queue.push(b);
						}
					}
				}
				pdata.blocking.clear();
				for (Edge_handle b : pdata.blocked_by) {
					utils::listRemove(prev, data_traits.data(b).blocking);
				}
				pdata.blocked_by.clear();
			}

			// clear blocking data from next edge
			{
				Edge_data& ndata = data_traits.data(next);
				for (Edge_handle b : ndata.blocking) {
					if (b == prev) continue; // NB: next could be blocking prev, but will be reset or removed
					auto& bdata = data_traits.data(b);
					if (utils::listRemove(next, bdata.blocked_by)) {
						if (bdata.blocked_by.empty() && !bdata.blocked_by_degzero) {
							queue.push(b);
						}
					}
				}
				ndata.blocking.clear();
				for (Edge_handle b : ndata.blocked_by) {
					utils::listRemove(next, data_traits.data(b).blocking);
				}
				ndata.blocked_by.clear();
			}

			Vertex_handle src = e->source();
			Vertex_handle tar = e->target();

			if (edata.erase_both) {

				graph.start_operation_group();

				graph.merge_vertex(src);
				Edge_handle ne = graph.merge_vertex(tar);

				graph.end_operation_group();

				// insert the one new edge
				sqt->insert(ne);

				// update it and its neighbors, if applicable
				update(ne);
				if (ne->source()->degree() == 2) {
					update(ne->prev());
				}
				if (ne->target()->degree() == 2) {
					update(ne->next());
				}
			}
			else {

				// perform the collapse
				Vertex_handle v = graph.collapse_edge(e, edata.point);

				// insert the two new edges
				sqt->insert(v->incoming());
				sqt->insert(v->outgoing());

				// update them and their neighbors, if applicable
				update(v->incoming());
				update(v->outgoing());

				if (v->prev()->degree() == 2) {
					update(v->prev()->incoming());
				}
				if (v->next()->degree() == 2) {
					update(v->next()->outgoing());
				}
			}
		}

	public:
		EdgeCollapse(Graph& g) requires std::is_default_constructible<Data_traits>::value&& std::is_default_constructible<Score_traits>::value : graph(g) {
		}

		EdgeCollapse(Graph& g, Score_traits score) requires std::is_default_constructible<Data_traits>::value : graph(g), score_traits(score) {
		}

		EdgeCollapse(Graph& g, Data_traits data) requires std::is_default_constructible<Score_traits>::value : graph(g), data_traits(data) {
		}

		EdgeCollapse(Graph& g, Data_traits data, Score_traits score) : graph(g), data_traits(data), score_traits(score) {
		}

		~EdgeCollapse() {
			delete sqt;
			delete pqt;
		}

		void initialize(const int& treedepth, const Number<Kernel> fuzziness) {
			data_traits.initialize(graph);
			score_traits.initialize(graph, data_traits);

			Rectangle<Kernel> box = graph.bounding_rectangle();

			sqt = new EdgeTree(box, treedepth, fuzziness);
			for (Edge_handle e : graph.edges()) {
				sqt->insert(e);
			}

			pqt = new VertexTree(box, treedepth);
			for (Vertex_handle v : graph.vertices()) {
				if (v->degree() == 0) {
					pqt->insert(v);
				}
			}

			for (Edge_handle e : graph.edges()) {
				update(e);
			}
		}

		bool run(std::function<bool(int, Number<Kernel>)> stop) {
			while (true) {
				Edge_handle next = findNextStep();
				if (next == nullptr) {
					return false;
				}

				if (stop(graph.number_of_edges(), data_traits.data(next).cost)) {
					return true;
				}

				performStep(next);
			}
		}

		bool step() {
			assert(graph.can_perform_operation());

			Edge_handle e = findNextStep();
			if (e == nullptr) {
				return false;
			}

			performStep(e);
			return true;
		}
	};

	template<DataTraits DT, bool ApproximatePoint = false>
	struct MinimumSymmetricDifferenceCollapse {
		using Graph = DT::Graph;
		using Edge_handle = Graph::Edge_handle;
		using Edge_data = DT::Edge_data;
		using Kernel = DT::Graph::Kernel;

		MinimumSymmetricDifferenceCollapse() {
		}

		static void initialize(Graph& graph, DT& data) {
			// skip
		}

		static void determine_collapse(Edge_handle e, Edge_data& edata) {
			Point<Kernel> a = e->prev()->source()->point();
			Point<Kernel> b = e->source()->point();
			Point<Kernel> c = e->target()->point();
			Point<Kernel> d = e->next()->target()->point();

			bool abc = safe_test::collinear(a, b, c);
			bool bcd = safe_test::collinear(b, c, d);

			if (abc && bcd) {
				edata.erase_both = true;
				edata.creates_difference = false;
				edata.cost = 0;
				return;
			}
			else if (abc) {
				edata.erase_both = false;
				edata.creates_difference = false;
				edata.cost = 0;
				edata.point = c;
				return;
			}
			else if (bcd) {
				edata.erase_both = false;
				edata.creates_difference = false;
				edata.cost = 0;
				edata.point = b;
				return;
			}

			// else, no consecutive collinear edges
			edata.creates_difference = true;

			Polygon<Kernel> P;
			P.push_back(a);
			P.push_back(b);
			P.push_back(c);
			P.push_back(d);

			Line<Kernel> ad(a, d);
			Line<Kernel> ab(a, b);
			Line<Kernel> bc(b, c);
			Line<Kernel> cd(c, d);

			// area = base * height / 2
			// height = 2*area / base
			// so, we're going to rotate the vector d-a, such that we get a normal of length |d-a| = base.
			// To get a vector of length height, we then multiply this vector with height_times_base / base^2.
			// This normalized the vector and makes it length height! (without squareroots...)
			Number<Kernel> height_times_base = 2 * P.area();

			Vector<Kernel> perpv = (d - a).perpendicular(CGAL::CLOCKWISE);

			CGAL::Aff_transformation_2<Kernel> s(CGAL::SCALING, height_times_base / perpv.squared_length());
			perpv = perpv.transform(s);

			CGAL::Aff_transformation_2<Kernel> t(CGAL::TRANSLATION, perpv);
			Line<Kernel> arealine = ad.transform(t);

			bool zero_area = safe_test::point_on_line(arealine.point(), ad);

			if (zero_area) {

				// these should be caught already by the collinearity checks earlier
				assert(!ad.has_on_boundary(b));
				assert(!ad.has_on_boundary(c));

				edata.erase_both = true;

				// implies that neither b nor c is on ad
				auto intersection = CGAL::intersection(bc, ad);
				Point<Kernel> pt = std::get<Point<Kernel>>(*intersection);

				edata.T1 = Triangle<Kernel>(a, b, pt);
				edata.T2 = Triangle<Kernel>(c, d, pt);
			}
			else {
				edata.erase_both = false;

				bool ab_determines_shape;
				// determine type
				if (ad.has_on_positive_side(b) == ad.has_on_positive_side(c)) {
					// same side of ab, so further point determines
					ab_determines_shape = CGAL::squared_distance(b, ad) > CGAL::squared_distance(c, ad);
				}
				else {
					// opposite sides of ad, so the one that is on same side as area line
					ab_determines_shape =
						ad.has_on_positive_side(b) == ad.has_on_positive_side(arealine.point());
				}

				// configure type
				if (ab_determines_shape) {

					auto intersection = CGAL::intersection(arealine, ab);
					edata.point = std::get<Point<Kernel>>(*intersection);

					Line<Kernel> ns = Line<Kernel>(edata.point, d);
					auto intersection2 = CGAL::intersection(bc, ns);
					Point<Kernel> is = std::get<Point<Kernel>>(*intersection2);

					edata.T1 = Triangle<Kernel>(b, is, edata.point);
					edata.T2 = Triangle<Kernel>(c, d, is);
				}
				else {

					auto intersection = CGAL::intersection(arealine, cd);
					edata.point = std::get<Point<Kernel>>(*intersection);

					Line<Kernel> ns = Line<Kernel>(edata.point, a);
					auto intersection2 = CGAL::intersection(bc, ns);
					Point<Kernel> is = std::get<Point<Kernel>>(*intersection2);

					edata.T1 = Triangle<Kernel>(a, b, is);
					edata.T2 = Triangle<Kernel>(c, is, edata.point);
				}

				if constexpr (ApproximatePoint) {
					edata.point = convert_kernel<Kernel>(approximate(edata.point));
				}
			}

			// since it is an area preserving method, T1 and T2 have the same area
			// NB: symmetric difference = twice this cost
			edata.cost = CGAL::abs(edata.T1.area());
		}
	};

} // namespace cartocrow::simplification::ec