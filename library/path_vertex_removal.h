#pragma once

#include "common.h"

namespace cartocrow::simplification::pvr {

	template<bool UseSelfVertexData, class ActualVertexData,
		class EdgeData,
		bool UseSelfPathData, class ActualPathData, 
		KernelType K, class GraphTraits, class DT, typename CustomVertexData, typename CustomPathData>
	struct PathData;

	template<bool UseSelfVertexData, class ActualVertexData,
		class EdgeData,
		bool UseSelfPathData, class ActualPathData,
		KernelType K, class GraphTraits, class DT, typename CustomVertexData, typename CustomPathData>
	struct VertexData {
		using Graph_vertex_data = std::conditional<UseSelfVertexData, VertexData, ActualVertexData>::type;
		using Graph_path_data = PathData<UseSelfVertexData, ActualVertexData, EdgeData, UseSelfPathData, ActualPathData, K, GraphTraits, DT, CustomVertexData, CustomPathData>::Graph_path_data;		

		using Kernel = K;
		using Vertex_handle = Graph_2_vertex<Graph_vertex_data, EdgeData, Graph_path_data, Graph_segment_curve_traits_2<K>, GraphTraits>*; // needed to avoid co-dependent templates for dedicated graphs

		bool left;
		std::vector<Vertex_handle> blocked_by;
		std::vector<Vertex_handle> blocking;
		Number<Kernel> cost;
		int queue_index = -1;

		CustomVertexData custom;
	};

	template<bool UseSelfVertexData, class ActualVertexData,
		class EdgeData,
		bool UseSelfPathData, class ActualPathData,
		KernelType K, class GraphTraits, class DT, typename CustomVertexData, typename CustomPathData>
	struct PathData {
		using Graph_vertex_data = VertexData<UseSelfVertexData, ActualVertexData, EdgeData, UseSelfPathData, ActualPathData, K, GraphTraits, DT, CustomVertexData, CustomPathData>::Graph_vertex_data;
		using Graph_path_data = std::conditional<UseSelfPathData, PathData, ActualPathData>::type;
	
		using Kernel = K;
		using Vertex_handle = Graph_2_vertex<Graph_vertex_data, EdgeData, Graph_path_data, Graph_segment_curve_traits_2<K>, GraphTraits>*; // needed to avoid co-dependent templates for dedicated graphs
		using VertexQueue = cartocrow::IndexedPriorityQueue<StatefulGraphQueueTraits<Vertex_handle, DT>>;

		bool next_from_left;
		VertexQueue* left_queue = nullptr;
		VertexQueue* right_queue = nullptr;
		Number<Kernel> cost;
		int queue_index = -1;

		CustomPathData custom;

		void initQueues(DT& dt) {
			delete left_queue;
			left_queue = new VertexQueue(dt);
			delete right_queue;
			right_queue = new VertexQueue(dt);
		}

		~PathData() {
			delete left_queue;
			delete right_queue;
		}
	};

	template<class DT>
	concept DataTraits =
		KernelType<typename DT::Kernel> &&
		GraphType<typename DT::Graph> &&
		DT::Graph::Graph_traits::oriented &&
		DT::Graph::Graph_traits::decomposed &&
		std::is_default_constructible<typename DT::Vertex_data>::value &&
		std::is_default_constructible<typename DT::Path_data>::value &&
		std::is_same<typename DT::Graph::Vertex_handle, typename DT::Vertex_data::Vertex_handle>::value &&
		std::is_same<typename DT::Graph::Vertex_handle, typename DT::Path_data::Vertex_handle>::value &&
		requires(DT data, typename DT::Graph graph, typename DT::Graph::Vertex_handle v, typename DT::Graph::Path_handle p) {

		typename DT::Kernel;
		typename DT::Graph;
		typename DT::Vertex_data;
		typename DT::Path_data;
		{ data.initialize(graph) };
		{ data.data(v) } -> std::same_as<typename DT::Vertex_data&>;
		{ data.data(p) } -> std::same_as<typename DT::Path_data&>;
	};

	template<class ST>
	concept ScoreTraits =
		KernelType<typename ST::Kernel> &&
		GraphType<typename ST::Graph> &&
		ST::Graph::Graph_traits::oriented &&
		ST::Graph::Graph_traits::decomposed &&
		requires(ST score, typename ST::Graph graph, typename ST::Graph::Vertex_handle v, typename ST::Graph::Path_handle p, typename ST::Vertex_data vdata, typename ST::Path_data pdata) {

		typename ST::Kernel;
		typename ST::Graph;
		typename ST::Vertex_data;
		typename ST::Path_data;
		//{ score.initialize(graph, data) }; Moved to general Setup concept, to not make this depend on DataTraits
		{ score.compute_cost(v, vdata) } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.compute_cost(p, pdata, v, vdata) } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.update(p, pdata, v, vdata) };
	};

	template<class DT, class ST>
	concept Setup =
		DataTraits<DT> && ScoreTraits<ST> &&
		std::is_same<typename ST::Kernel, typename DT::Kernel>::value&&
		std::is_same<typename ST::Graph, typename DT::Graph>::value&&
		std::is_same<typename ST::Vertex_data, typename DT::Vertex_data>::value&&
		std::is_same<typename ST::Path_data, typename DT::Path_data>::value&&
		requires(ST score, DT data, typename DT::Graph graph) {

			{ score.initialize(graph, data) };
	};

	template<KernelType K, bool Historic, class CustomPathData = std::monostate, class CustomVertexData = std::monostate>
	class DataTraits_Dedicated {
	public:
		using Kernel = K;
		using Vertex_data = VertexData<true, std::monostate, std::monostate, true, std::monostate, Kernel, DecomposedGraph<Historic>, DataTraits_Dedicated, CustomVertexData, CustomPathData>;
		using Path_data = PathData<true, std::monostate, std::monostate, true, std::monostate, Kernel, DecomposedGraph<Historic>, DataTraits_Dedicated, CustomVertexData, CustomPathData>;
		using Graph = Straight_graph_2<Vertex_data, std::monostate, Path_data, Kernel, DecomposedGraph<Historic>>;

		static void initialize(Graph& graph) {
			// skip
		}

		static Vertex_data& data(typename Graph::Vertex_handle v) {
			return v->data();
		}

		static Path_data& data(typename Graph::Path_handle p) {
			return p->data();
		}
	};

	template<GraphType G, class CustomPathData = std::monostate, class CustomVertexData = std::monostate>
	class DataTraits_Generic {
	public:
		using Graph = G;
		using Kernel = Graph::Kernel;
		using Vertex_data = VertexData<false, typename G::Vertex_data, typename G::Edge_data, false, typename G::Path_data, Kernel, typename G::Graph_traits, DataTraits_Generic, CustomVertexData, CustomPathData>;
		using Path_data = PathData<false, typename G::Vertex_data, typename G::Edge_data, false, typename G::Path_data, Kernel, typename G::Graph_traits, DataTraits_Generic, CustomVertexData, CustomPathData>;

		void initialize(Graph& graph) {
			delete m_vertex_data;
			delete m_path_data;
			m_vertex_data = new Graph_vertex_map<Graph, Vertex_data>(graph, Vertex_data());
			m_path_data = new Graph_path_map<Graph, Path_data>(graph, Path_data());
		}

		~DataTraits_Generic() {
			delete m_vertex_data;
			delete m_path_data;
		}

		Vertex_data& data(typename Graph::Vertex_handle v) {
			return (*m_vertex_data)[v];
		}

		Path_data& data(typename Graph::Path_handle p) {
			return (*m_path_data)[p];
		}
	private:
		Graph_vertex_map<Graph, Vertex_data>* m_vertex_data = nullptr;
		Graph_path_map<Graph, Path_data>* m_path_data = nullptr;
	};

	template <DataTraits DT, ScoreTraits ST>
		requires Setup<DT, ST>
	class PathVertexRemoval {
	public:
		using Data_traits = DT;
		using Score_traits = ST;

		using Graph = Data_traits::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Vertex_data = Data_traits::Vertex_data;
		using Path_handle = Graph::Path_handle;
		using Path_data = Data_traits::Path_data;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;

	private:
		using VertexQueue = cartocrow::IndexedPriorityQueue<StatefulGraphQueueTraits<Vertex_handle, Data_traits>>;
		using PathQueue = cartocrow::IndexedPriorityQueue<StatefulGraphQueueTraits<Path_handle, Data_traits>>;

		Graph& graph;
		VertexTree* pqt = nullptr;
		Data_traits data_traits;
		Score_traits score_traits;
		PathQueue queue = PathQueue(data_traits);

		VertexQueue& left_queue(Path_data& pd) {
			return *static_cast<VertexQueue*>(pd.left_queue);
		}

		VertexQueue& right_queue(Path_data& pd) {
			return *static_cast<VertexQueue*>(pd.left_queue);
		}

		VertexQueue& path_queue(Path_data& pd, const bool& left) {
			return left ? left_queue(pd) : right_queue(pd);
		}

		void update(Vertex_handle v) {

			if (v->degree() != 2) {
				return;
			}

			Vertex_data& vdata = data_traits.data(v);

			// clear topology
			for (void* bvoid : vdata.blocked_by) {
				Vertex_handle b = static_cast<Vertex_handle>(bvoid);
				utils::listRemove(v, data_traits.data(b).blocking);
			}
			vdata.blocked_by.clear();

			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			Path_handle p = v->outgoing()->path();
			Path_data& pdata = data_traits.data(p);

			if (u->is_neighbor_of(w)) {
				path_queue(pdata, vdata.left).remove(v);
				return;
			}

			vdata.left = CGAL::left_turn(u->point(), v->point(), w->point());
			vdata.cost = score_traits.compute_cost(v, vdata);

			path_queue(pdata, !vdata.left).remove(v);
			VertexQueue& q = path_queue(pdata, vdata.left);
			if (q.contains(v)) {
				q.update(v);
			}
			else {
				q.push(v);
			}
		}

		void update(Path_handle p) {
			update(p, data_traits.data(p));
		}
		void update(Path_handle p, Path_data& pdata) {

			VertexQueue& left_q = left_queue(pdata);
			VertexQueue& right_q = right_queue(pdata);

			if (!left_q.empty()) {
				Vertex_handle left_v = left_q.peek();
				pdata.cost = score_traits.compute_cost(p, pdata, left_v, data_traits.data(left_v));

				if (!right_q.empty()) {
					Vertex_handle right_v = right_q.peek();
					Number<Kernel> cost = score_traits.compute_cost(p, pdata, right_v, data_traits.data(right_v));

					if (cost < pdata.cost) {
						pdata.next_from_left = false;
						pdata.cost = cost;
					}
					else {
						pdata.next_from_left = true;
					}
				}
				else {
					pdata.next_from_left = true;
				}
			}
			else if (!right_q.empty()) {
				Vertex_handle right_v = right_q.peek();
				pdata.cost = score_traits.compute_cost(p, pdata, right_v, data_traits.data(right_v));
				pdata.next_from_left = false;
			}
			else {
				// no operation
				queue.remove(p);
				return;
			}

			if (queue.contains(p)) {
				queue.update(p);
			}
			else {
				queue.push(p);
			}
		}

		Vertex_handle findNextStep() {
			assert(graph.can_perform_operation());

			while (!queue.empty()) {
				Path_handle p = queue.peek();
				Path_data& pdata = data_traits.data(p);

				Vertex_handle v = path_queue(pdata, pdata.next_from_left).peek();
				Vertex_data& vdata = data_traits.data(v);

				Vertex_handle u = v->prev();
				Vertex_handle w = v->next();

				// test whether the operation is blocked
				const Point<Kernel>& up = u->point();
				const Point<Kernel>& vp = v->point();
				const Point<Kernel>& wp = w->point();
				Triangle<Kernel> T(up, vp, wp);

				Rectangle<Kernel> rect = utils::boxOf(up, vp, wp);

				pqt->findContained(rect, [&T, u, v, w, &vdata, this](Vertex_handle b) {
					if (b != u && b != v && b != w && !T.has_on_unbounded_side(b->point())) {
						// blocked, record the pair
						Vertex_data& bd = data_traits.data(b);
						bd.blocking.push_back(v);
						vdata.blocked_by.push_back(b);
					}
					});


				if (vdata.blocked_by.empty()) {
					// not blocked, this is the next step
					return v;
				}
				else {
					// remove the element from the queue as it's not valid and continue searching
					path_queue(pdata, pdata.next_from_left).pop();
					update(p, pdata);
				}
			}

			// no valid steps exist
			return nullptr;
		}

		void performStep(Vertex_handle v) {
			//std::cout << "performing " << *v << std::endl;

			assert(graph.can_perform_operation());

			Vertex_data& vdata = data_traits.data(v);

			Path_handle p = v->outgoing()->path();
			Path_data& pdata = data_traits.data(p);

			assert(queue.peek() == p);
			assert(pdata.next_from_left == vdata.left);

			path_queue(pdata, vdata.left).pop();

			// remove from blocking lists and search structure
			pqt->remove(v);
			for (void* bvoid : vdata.blocking) {
				Vertex_handle b = static_cast<Vertex_handle>(bvoid);
				Vertex_data& bd = data_traits.data(b);

				if (utils::listRemove(v, bd.blocked_by)) {
					if (bd.blocked_by.empty()) {
						Path_data& bpd = data_traits.data(b->outgoing()->path());

						path_queue(bpd, bd.left).push(b);

						if (&bpd != &pdata) {
							update(b->outgoing()->path(), bpd);
						}
					}
				}
			}

			score_traits.update(p, pdata, v, vdata);

			// perform the removal
			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			graph.merge_vertex(v);

			// update the neighbors
			update(u);
			update(w);

			// and their common neighbors (to avoid issues with triangles collapsing)
			for (int i = 0; i < u->degree(); i++) {
				Vertex_handle nbr = u->neighbor(i);
				if (nbr->is_neighbor_of(w)) {
					update(nbr);
					if (nbr->outgoing()->path() != p) {
						update(nbr->outgoing()->path());
					}
				}
			}

			update(p, pdata);
		}
	public:
		PathVertexRemoval(Graph& g) requires std::is_default_constructible<Data_traits>::value&& std::is_default_constructible<Score_traits>::value : graph(g) {
		}

		PathVertexRemoval(Graph& g, Score_traits score) requires std::is_default_constructible<Data_traits>::value : graph(g), score_traits(score) {
		}

		PathVertexRemoval(Graph& g, Data_traits data) requires std::is_default_constructible<Score_traits>::value : graph(g), data_traits(data) {
		}

		PathVertexRemoval(Graph& g, Data_traits data, Score_traits score) : graph(g), data_traits(data), score_traits(score) {
		}

		~PathVertexRemoval() {
			delete pqt;
		}

		void initialize(const int tree_depth) {
			data_traits.initialize(graph);
			score_traits.initialize(graph, data_traits);

			pqt = new VertexTree(graph.bounding_rectangle(), tree_depth);

			for (Path_handle p : graph.paths()) {
				Path_data& pd = data_traits.data(p);
				pd.initQueues(data_traits);
			}

			for (Vertex_handle v : graph.vertices()) {
				pqt->insert(v);
				update(v);
			}

			for (Path_handle p : graph.paths()) {
				update(p);
			}
		}

		bool run(std::function<bool(int, Number<Kernel>)> stop) {
			while (true) {
				Vertex_handle next = findNextStep();
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

			Vertex_handle v = findNextStep();
			if (v == nullptr) {
				return false;
			}

			performStep(v);
			return true;
		}
	};

	template<DataTraits DT>
//	requires std::is_same<decltype(DT::Path_data::custom), Number<typename DT::Kernel>>::value
	class BiasedEffectiveAreaScore {
		// Custom field is used to store a number in a path: "left area increased".
	public:
		using Graph = DT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Vertex_data = DT::Vertex_data;
		using Path_handle = Graph::Path_handle;
		using Path_data = DT::Path_data;
		using Kernel = DT::Graph::Kernel;

		BiasedEffectiveAreaScore() : m_weight(1) {
		}
		BiasedEffectiveAreaScore(Number<Kernel> weight) : m_weight(weight) {
		}

		static void initialize(Graph& graph, DT& data) {
			// skip
		}

		static Number<Kernel> compute_cost(Vertex_handle v, Vertex_data& vd) {
			return CGAL::abs(
				CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
		}

		Number<Kernel> compute_cost(Path_handle p, Path_data& pd, Vertex_handle v, Vertex_data& vd) {
			return vd.cost + (vd.left ? -m_weight : m_weight) * pd.custom;
		}

		static void update(Path_handle p, Path_data& pd, Vertex_handle v, Vertex_data& vd) {
			if (vd.left) {
				pd.custom -= vd.cost;
			}
			else {
				pd.custom += vd.cost;
			}
		}
	private:
		Number<Kernel> m_weight;
	};

} // namespace cartocrow::simplification::pvr