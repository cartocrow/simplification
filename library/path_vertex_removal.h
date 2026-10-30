#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/straight_graph_2.h>

#include "vertex_quad_tree.h"
#include "common.h"
#include "utils.h"

namespace cartocrow::simplification::pvr {
			
		template<KernelType K, class Base = EmptyBase>
		struct VertexData : public Base {
			using Kernel = K;
			using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs

			bool left;
			std::vector<Vertex_handle> blocked_by;
			std::vector<Vertex_handle> blocking;
			Number<Kernel> cost;
			int queue_index = -1;
		};

		template<KernelType K, class Base = EmptyBase>
		struct PathData : public Base {
			using Kernel = K;
			using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs
			using Path_handle = void*; // needed to avoid co-dependent templates for dedicated graphs
			using Queue = void*; // needed to avoid co-dependent templates for dedicated graphs (?)

			Path_handle path;
			bool next_from_left;
			Queue left_queue = nullptr, right_queue = nullptr;
			Number<Kernel> cost;
			int queue_index = -1;

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
			std::is_same<typename ST::Kernel, typename DT::Kernel>::value &&
			std::is_same<typename ST::Graph, typename DT::Graph>::value &&
			std::is_same<typename ST::Vertex_data, typename DT::Vertex_data>::value &&
			std::is_same<typename ST::Path_data, typename DT::Path_data>::value &&
			requires(ST score, DT data, typename DT::Graph graph) {

			{ score.initialize(graph, data) };
		};

		template<KernelType K, bool Historic, class PathBase = EmptyBase, class VertexBase = EmptyBase>
		class DataTraits_Dedicated {
		public:
			using Kernel = K;
			using Vertex_data = VertexData<Kernel, VertexBase>;
			using Path_data = PathData<Kernel, PathBase>;
			using Graph = Straight_graph_2<Vertex_data, std::monostate, Kernel, DecomposedGraph<Historic, Path_data>>;

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

		template<GraphType G, class PathBase = EmptyBase, class VertexBase = EmptyBase>
		class DataTraits_Generic {
		public:
			using Graph = G;
			using Kernel = Graph::Kernel;
			using Vertex_data = VertexData<Kernel, VertexBase>;
			using Path_data = PathData<Kernel, PathBase>;

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
				delete pd.left_queue;
				pd.left_queue = new VertexQueue(data_traits);
				delete pd.right_queue;
				pd.right_queue = new VertexQueue(data_traits);
			}

			for (Vertex_handle v : graph.vertices()) {
				pqt->insert(v);
				update(v);
			}

			for (Path_handle p : graph.paths()) {
				update(p);
			}
		}

		bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt) {
			while (true) {
				Vertex_handle next = findNextStep();
				if (next == nullptr) {
					return false;
				}

				if (!stop.has_value() || (*stop)(graph.number_of_edges(), data_traits.data(next).cost)) {
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

	template<KernelType Kernel>
	struct BiasedEffectiveArea_PathBase {
		Number<Kernel> left_area_increased;
	};

	template<DataTraits DT>
	requires std::is_base_of<BiasedEffectiveArea_PathBase<typename DT::Kernel>, typename DT::Path_data>::value
	class BiasedEffectiveAreaScore {
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
			return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
		}

		static void update(Path_handle p, Path_data& pd, Vertex_handle v, Vertex_data& vd) {
			if (vd.left) {
				pd.left_area_increased -= vd.cost;
			}
			else {
				pd.left_area_increased += vd.cost;
			}
		}
	private:
		Number<Kernel> m_weight;
	};

} // namespace cartocrow::simplification::pvr