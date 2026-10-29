#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>

#include "vertex_quad_tree.h"
#include "common.h"
#include "utils.h"

namespace cartocrow::simplification::vr {

	template<KernelType K, class Base>
	struct VertexData : public Base {
		using Kernel = K;
		using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs

		std::vector<Vertex_handle> blocked_by;
		std::vector<Vertex_handle> blocking;
		Number<Kernel> cost;
		int queue_index = -1;
	};

	template<class DT>
	concept DataTraits =
		KernelType<typename DT::Kernel> &&
		GraphType<typename DT::Graph> &&
		DT::Graph::Graph_traits::oriented &&
		DT::Graph::Graph_traits::decomposed &&
		std::is_default_constructible<typename DT::Vertex_data>::value &&
		requires(DT data, typename DT::Graph graph, typename DT::Graph::Vertex_handle v) {

		typename DT::Kernel;
		typename DT::Graph;
		typename DT::Vertex_data;
		{ data.initialize(graph) };
		{ data.data(v) } -> std::same_as<typename DT::Vertex_data&>;
	};

	template<class ST>
	concept ScoreTraits =
		KernelType<typename ST::Kernel> &&
		GraphType<typename ST::Graph> &&
		ST::Graph::Graph_traits::oriented &&
		ST::Graph::Graph_traits::decomposed &&
		requires(ST score, typename ST::Graph graph, typename ST::Vertex_handle v, typename ST::Vertex_data& vdata) {

		typename ST::Kernel;
		typename ST::Graph;
		typename ST::Vertex_data;
		//{ score.initialize(graph, data) }; Moved to general Setup concept, to not make this depend on DataTraits
		{ score.compute_cost(v, vdata) } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.update(v, vdata) };
	};

	template<class DT, class ST>
	concept Setup =
		DataTraits<DT> && ScoreTraits<ST> &&
		std::is_same<typename ST::Kernel, typename DT::Kernel>::value&&
		std::is_same<typename ST::Graph, typename DT::Graph>::value&&
		std::is_same<typename ST::Vertex_data, typename DT::Vertex_data>::value&&
		requires(ST score, DT data, typename DT::Graph graph) {

			{ score.initialize(graph, data) };
	};

	template<KernelType K, bool Historic, class VertexBase = EmptyBase>
	class DataTraits_Dedicated {
	public:
		using Kernel = K;
		using Vertex_data = VertexData<Kernel, VertexBase>;
		using Graph = Straight_graph_2<Vertex_data, std::monostate, Kernel, DecomposedGraph<Historic, std::monostate>>;

		static void initialize(Graph& graph) {
			// skip
		}

		static Vertex_data& data(typename Graph::Vertex_handle v) {
			return v->data();
		}
	};

	template<GraphType G, class VertexBase = EmptyBase>
	class DataTraits_Generic {
	public:
		using Graph = G;
		using Kernel = Graph::Kernel;
		using Vertex_data = VertexData<Kernel, VertexBase>;

		void initialize(Graph& graph) {
			delete m_vertex_data;
			m_vertex_data = new Graph_vertex_map<Graph, Vertex_data>(graph, Vertex_data());
		}

		~DataTraits_Generic() {
			delete m_vertex_data;
		}

		Vertex_data& data(typename Graph::Vertex_handle v) {
			return (*m_vertex_data)[v];
		}
	private:
		Graph_vertex_map<Graph, Vertex_data>* m_vertex_data = nullptr;
	};

	template <DataTraits DT, ScoreTraits ST>
		requires Setup<DT, ST>
	class VertexRemoval {
	public:
		using Data_traits = DT;
		using Score_traits = ST;

		using Graph = Data_traits::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Vertex_data = Data_traits::Vertex_data;
		using Kernel = Graph::Kernel;

	private:
		using VertexTree = VertexQuadTree<Graph>;
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<StatefulGraphQueueTraits<Vertex_handle, Data_traits>>;

		Graph& graph;
		VertexTree* pqt = nullptr;
		Data_traits data_traits;
		Score_traits score_traits;
		Queue queue = Queue(data_traits);

		void update(Vertex_handle v) {
			if (v->degree() != 2) {
				return;
			}

			Vertex_data& vdata = data_traits.data(v);

			// clear topology
			for (void* bvoid : vdata.blocked_by) {
				utils::listRemove(v, data_traits.data(static_cast<Vertex_handle>(bvoid)).blocking);
			}
			vdata.blocked_by.clear();

			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			if (u->is_neighbor_of(w)) {
				queue.remove(v);
				return;
			}

			vdata.cost = score_traits.compute_cost(v, vdata);

			if (queue.contains(v)) {
				queue.update(v);
			}
			else {
				queue.push(v);
			}
		}

		Vertex_handle findNextStep() {
			assert(graph.can_perform_operation());

			while (!queue.empty()) {

				Vertex_handle v = queue.peek();
				Vertex_handle u = v->prev();
				Vertex_handle w = v->next();

				// test whether the operation is blocked
				const Point<Kernel>& up = u->point();
				const Point<Kernel>& vp = v->point();
				const Point<Kernel>& wp = w->point();
				Triangle<Kernel> T(up, vp, wp);

				Rectangle<Kernel> rect = utils::boxOf(up, vp, wp);

				Vertex_data& vdata = data_traits.data(v);

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
					queue.pop();
				}
			}

			// no valid steps exist
			return nullptr;
		}

		void performStep(Vertex_handle v) {

			assert(graph.can_perform_operation());
			assert(queue.peek() == v);

			queue.pop();

			Vertex_data& vdata = data_traits.data(v);

			// remove from blocking lists and search structure
			pqt->remove(v);
			for (void* bvoid : vdata.blocking) {
				Vertex_handle b = static_cast<Vertex_handle>(bvoid);
				Vertex_data& bdata = data_traits.data(b);
				utils::listRemove(v, bdata.blocked_by);
				if (bdata.blocked_by.empty()) {
					queue.push(b);
				}
			}

			score_traits.update(v, vdata);

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
				}
			}
		}
	public:
		VertexRemoval(Graph& g) requires std::is_default_constructible<Data_traits>::value&& std::is_default_constructible<Score_traits>::value : graph(g) {
		}

		VertexRemoval(Graph& g, Score_traits score) requires std::is_default_constructible<Data_traits>::value : graph(g), score_traits(score) {
		}

		VertexRemoval(Graph& g, Data_traits data) requires std::is_default_constructible<Score_traits>::value : graph(g), data_traits(data) {
		}

		VertexRemoval(Graph& g, Data_traits data, Score_traits score) : graph(g), data_traits(data), score_traits(score) {
		}

		~VertexRemoval() {
			delete pqt;
		}

		void initialize(const int tree_depth) {
			data_traits.initialize(graph);
			score_traits.initialize(graph, data_traits);

			pqt = new VertexTree(graph.bounding_rectangle(), tree_depth);
			for (Vertex_handle v : graph.vertices()) {
				pqt->insert(v);
				update(v);
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

	template<DataTraits DT>
	class EffectiveAreaScore {
	public:
		using Graph = DT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Vertex_data = DT::Vertex_data;
		using Kernel = DT::Graph::Kernel;

		static void initialize(Graph& graph, DT& data) {
			// skip
		}

		static Number<Kernel> compute_cost(Vertex_handle v, Vertex_data& vdata) {
			return CGAL::abs(
				CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
		}


		static void update(Vertex_handle v, Vertex_data& vdata) {
			// skip
		}
	};

} // namespace cartocrow::simplification::vr