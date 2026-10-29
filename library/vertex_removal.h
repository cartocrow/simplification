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
		using Vertex_data_handle = VertexData<K, Base>*;
		using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs

		Vertex_handle vertex;
		std::vector<Vertex_data_handle> blocked_by;
		std::vector<Vertex_data_handle> blocking;
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
		requires(ST score, typename ST::Graph graph, typename ST::Vertex_data vdata) {

		typename ST::Kernel;
		typename ST::Graph;
		typename ST::Vertex_data;
		//{ score.initialize(graph, data) }; Moved to general Setup concept, to not make this depend on DataTraits
		{ score.compute_cost(vdata) } -> std::same_as<Number<typename ST::Kernel>>;
		{ score.update(vdata) };
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
			m_vertex_data = new Graph_vertex_map<Graph, std::shared_ptr<Vertex_data>>(graph, nullptr);
			for (typename Graph::Vertex_handle v : graph.vertices()) {
				(*m_vertex_data)[v] = std::make_shared<Vertex_data>();
			}
		}

		~DataTraits_Generic() {
			delete m_vertex_data;
		}

		Vertex_data& data(typename Graph::Vertex_handle v) {
			return *(*m_vertex_data)[v].get();
		}
	private:
		Graph_vertex_map<Graph, std::shared_ptr<Vertex_data>>* m_vertex_data = nullptr;
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
		using VertexTree = VertexQuadTree<Graph>;

	private:
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<DataQueueTraits<Vertex_data*>>;

		Graph& graph;
		VertexTree* pqt = nullptr;
		Queue queue;
		Data_traits data_traits;
		Score_traits score_traits;

		void update(Vertex_handle v) {
			update(v, data_traits.data(v));
		}
		void update(Vertex_data& vdata) {
			update(static_cast<Vertex_handle>(vdata.vertex), vdata);
		}
		void update(Vertex_handle v, Vertex_data& vdata) {
			if (v->degree() != 2) {
				return;
			}

			// clear topology
			for (Vertex_data* bd : vdata.blocked_by) {
				utils::listRemove(&vdata, bd->blocking);
			}
			vdata.blocked_by.clear();

			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			if (u->is_neighbor_of(w)) {
				queue.remove(&vdata);
				return;
			}

			vdata.cost = score_traits.compute_cost(vdata);

			if (queue.contains(&vdata)) {
				queue.update(&vdata);
			}
			else {
				queue.push(&vdata);
			}
		}

		Vertex_data* findNextStep() {
			assert(graph.can_perform_operation());

			while (!queue.empty()) {

				Vertex_data* vd = queue.peek();

				Vertex_handle v = static_cast<Vertex_handle>(vd->vertex);
				Vertex_handle u = v->prev();
				Vertex_handle w = v->next();

				// test whether the operation is blocked
				const Point<Kernel>& up = u->point();
				const Point<Kernel>& vp = v->point();
				const Point<Kernel>& wp = w->point();
				Triangle<Kernel> T(up, vp, wp);

				Rectangle<Kernel> rect = utils::boxOf(up, vp, wp);

				pqt->findContained(rect, [&T, u, v, w, vd, this](Vertex_handle b) {
					if (b != u && b != v && b != w && !T.has_on_unbounded_side(b->point())) {
						// blocked, record the pair
						Vertex_data& bd = data_traits.data(b);
						bd.blocking.push_back(vd);
						vd->blocked_by.push_back(&bd);
					}
					});


				if (vd->blocked_by.empty()) {
					// not blocked, this is the next step
					return vd;
				}
				else {
					// remove the element from the queue as it's not valid and continue searching
					queue.pop();
				}
			}

			// no valid steps exist
			return nullptr;
		}

		bool validate(std::string tag) {

			bool failed = false;
			for (Vertex_handle v : graph.vertices()) {
				Vertex_data& vd = data_traits.data(v);
				if (vd.vertex != v) {
					std::cout << "err data" << "\n";
					failed = true;
					break;
				}
			}

			for (Vertex_data* vdp : queue.content()) {
				Vertex_handle vv = static_cast<Vertex_handle>(vdp->vertex);
				if (graph.vertex(vv->graph_index()) != vv) {
					std::cout << "err queue" << "\n";
					failed = true;
					break;
				}
			}

			if (failed) {
				std::cout << "failed @ " << graph.number_of_edges() << " , " << tag << "\n";
			}
			return !failed;

		}

		void performStep(Vertex_data& vd) {

			assert(graph.can_perform_operation());

			//validate("START");

			Vertex_handle v = static_cast<Vertex_handle>(vd.vertex);

			assert(queue.peek() == &vd);

			queue.pop();

			//validate("POP");

			// remove from blocking lists and search structure
			pqt->remove(v);
			for (Vertex_data* bd : vd.blocking) {
				utils::listRemove(&vd, bd->blocked_by);
				if (bd->blocked_by.empty()) {
					queue.push(bd);
				}
			}

			score_traits.update(vd);

			// perform the removal
			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			//validate("PRE PERFORM");

			graph.merge_vertex(v);

			//validate("POST MERGE");

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

			//if (!validate("POST UPDATES")) {
			//	queue.clear();
			//}
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

				Vertex_data& vd = data_traits.data(v);
				vd.vertex = v;
				update(v, vd);
			}
		}

		bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt) {
			while (true) {
				Vertex_data* next = findNextStep();
				if (next == nullptr) {
					return false;
				}

				if (!stop.has_value() || (*stop)(graph.number_of_edges(), next->cost)) {
					return true;
				}

				performStep(*next);
			}
		}

		bool step() {
			assert(graph.can_perform_operation());

			Vertex_data* vd = findNextStep();
			if (vd == nullptr) {
				return false;
			}

			performStep(vd);
			return true;
		}
	};

} // namespace cartocrow::simplification::vr


namespace cartocrow::simplification::vr2 {

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

} // namespace cartocrow::simplification::vr2

namespace cartocrow::simplification {

	namespace detail {

		template<bool H>
		using VRGraphTraits = DecomposedGraph<H, std::monostate>;

		template<typename K, bool H>
		struct VRData;

		template<class G>
		struct VRTraitsBase;

		template<class VRT>
		concept VRTraits = requires(typename VRT::Graph::Vertex_handle v) {

			typename VRT::Graph;

			{ VRT::data(v) } -> std::same_as<VRData<typename VRT::Graph::Kernel, VRT::Graph::Graph_traits::historic>&>;
			{ VRT::compute_cost(v) } -> std::same_as<Number<typename VRT::Graph::Kernel>>;
		};
	}

	template<typename K, bool H>
	using VertexRemovalGraph = Straight_graph_2<detail::VRData<K, H>, std::monostate, K, detail::VRGraphTraits<H>>;

	/// <summary>
	/// The Vertex Removal algorithm. It is topologically safe, ensuring that vertices are only erased if they have degree 2 and the triangle spanned with its neighbors is empty. Can be configured with custom cost function, via the VertexRemovalTraits.
	/// </summary>
	/// <typeparam name="MG">Modifiable Graph type to be used; typically, will be one of VertexRemovalGraph or HistoricVertexRemovalGraph</typeparam>
	/// <typeparam name="VRT">VertexRemovalTraits, specifying the desired cost function</typeparam>
	template <detail::VRTraits VRT> class VertexRemoval {

	public:
		using Graph = VRT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;

	private:
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<GraphQueueTraits<Vertex_handle>>;

		Graph& graph;
		VertexTree& pqt;
		Queue queue;

		void update(Vertex_handle v);

		Vertex_handle findNextStep();
		void performStep(Vertex_handle v);
	public:
		VertexRemoval(Graph& g, VertexTree& qt);

		void initialize(bool initQuadTree);
		bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt);
		bool step();
	};


	/// <summary>
	/// Traits for running VisvalingamWhyatt vertex-removal algorithms. The cost is equal to the area of the spanned triangle.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <class G>
	struct VisvalingamWhyattTraits : detail::VRTraitsBase<G> {
		using Kernel = G::Kernel;

		static Number<Kernel> compute_cost(typename G::Vertex_handle v) {
			return CGAL::abs(
				CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
		}
	};

	/// <summary>
	/// Shorthand for the VisvalingamWhyatt vertex-removal algorithm.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <class G>
	using VisvalingamWhyatt = VertexRemoval<VisvalingamWhyattTraits<G>>;

} // namespace cartocrow::simplification

#include "vertex_removal.hpp"