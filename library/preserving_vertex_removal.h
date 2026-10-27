#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>
#include <cartocrow/data_structures/graph_map_2.h>

#include "vertex_quad_tree.h"
#include "common.h"
#include "utils.h"

namespace cartocrow::simplification {

	template<class K>
	concept KernelType = std::is_same<K, Inexact>::value || std::is_same<K, Exact>::value;

	template<class G>
	concept GraphType = requires {
		typename G::Graph_traits;
	};

	namespace pvr {

		struct EmptyBase {
		};

		template<KernelType K, class Base>
		struct VertexData : public Base {
			using Kernel = K;
			using Vertex_data_handle = VertexData<K, Base>*;
			using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs

			Vertex_handle vertex;
			bool left;
			std::vector<Vertex_data_handle> blocked_by;
			std::vector<Vertex_data_handle> blocking;
			Number<Kernel> cost;
			int queue_index = -1;
		};

		template<KernelType K, class Base, class VertexBase>
		struct PathData : public Base {
			using Kernel = K;
			using Vertex_handle = void*; // needed to avoid co-dependent templates for dedicated graphs
			using Path_handle = void*; // needed to avoid co-dependent templates for dedicated graphs
			using Vertex_data = VertexData<Kernel, VertexBase>;
			using Queue = cartocrow::data_structures::IndexedPriorityQueue<DataQueueTraits<Vertex_data*>>;

			Path_handle path;
			Vertex_data* next_vertex = nullptr;
			Queue left_queue, right_queue;
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
			requires(ST score, typename ST::Graph graph, typename ST::Vertex_data vdata, typename ST::Path_data pdata) {

			typename ST::Kernel;
			typename ST::Graph;
			typename ST::Vertex_data;
			typename ST::Path_data;
			//{ score.initialize(graph, data) }; Moved to general Setup concept, to not make this depend on DataTraits
			{ score.compute_cost(vdata) } -> std::same_as<Number<typename ST::Kernel>>;
			{ score.compute_cost(pdata, vdata) } -> std::same_as<Number<typename ST::Kernel>>;
			{ score.update(pdata, vdata) };
		};

		template<class DT, class ST>
		concept PathVertexRemovalSetup =
			DataTraits<DT> && ScoreTraits<ST> &&
			std::is_same<typename ST::Kernel, typename DT::Kernel>::value &&
			std::is_same<typename ST::Graph, typename DT::Graph>::value &&
			std::is_same<typename ST::Vertex_data, typename DT::Vertex_data>::value &&
			std::is_same<typename ST::Path_data, typename DT::Path_data>::value &&
			requires(ST score, DT data, typename DT::Graph graph) {

			{ score.initialize(graph, data) };
		};

		template<KernelType K, bool Historic, class VertexBase = EmptyBase, class PathBase = EmptyBase>
		class GraphDataTraits {
		public:
			using Kernel = K;
			using Vertex_data = VertexData<Kernel, VertexBase>;
			using Path_data = PathData<Kernel, PathBase, VertexBase>;
			using Graph = Straight_graph_2<Vertex_data, std::monostate, Kernel, DecomposedGraph<Historic, Path_data>>;

			virtual void initialize(Graph& graph) {
				// skip
			}

			static Vertex_data& data(typename Graph::Vertex_handle v) {
				return v->data();
			}

			static Path_data& data(typename Graph::Path_handle p) {
				return p->data();
			}
		};

		template<GraphType G, class VertexBase = EmptyBase, class PathBase = EmptyBase>
		class GenericDataTraits {
		public:
			using Graph = G;
			using Kernel = Graph::Kernel;
			using Vertex_data = VertexData<Kernel, VertexBase>;
			using Path_data = PathData<Kernel, PathBase, VertexBase>;

			virtual void initialize(Graph& graph) {
				delete m_vertex_data;
				delete m_path_data;
				m_vertex_data = new Graph_vertex_map<Graph, Vertex_data>(graph, Vertex_data());
				m_path_data = new Graph_path_map<Graph, Path_data>(graph, Path_data());
			}

			~GenericDataTraits() {
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
	} // namespace pvr

	template <pvr::DataTraits DT, pvr::ScoreTraits ST>
		requires pvr::PathVertexRemovalSetup<DT, ST>
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
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<DataQueueTraits<Path_data*>>;

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

			Path_handle p = v->outgoing()->path();
			auto& pdata = data_traits.data(p);

			if (u->is_neighbor_of(w)) {
				if (vdata.left) {
					pdata.left_queue.remove(&vdata);
				}
				else {
					pdata.right_queue.remove(&vdata);
				}
				return;
			}

			vdata.left = CGAL::left_turn(u->point(), v->point(), w->point());
			vdata.cost = score_traits.compute_cost(vdata);

			if (vdata.left) {
				pdata.right_queue.remove(&vdata);
				if (pdata.left_queue.contains(&vdata)) {
					pdata.left_queue.update(&vdata);
				}
				else {
					pdata.left_queue.push(&vdata);
				}
			}
			else {
				pdata.left_queue.remove(&vdata);
				if (pdata.right_queue.contains(&vdata)) {
					pdata.right_queue.update(&vdata);
				}
				else {
					pdata.right_queue.push(&vdata);
				}
			}
		}

		void update(Path_handle p) {
			update(data_traits.data(p));
		}
		void update(Path_data& pdata) {

			if (pdata.left_queue.empty() && pdata.right_queue.empty()) {
				// no operation
				queue.remove(&pdata);
				return;
			}

			if (pdata.left_queue.empty()) {
				pdata.next_vertex = pdata.right_queue.peek();
				pdata.cost = score_traits.compute_cost(pdata, *pdata.next_vertex);
			}
			else if (pdata.right_queue.empty()) {
				pdata.next_vertex = pdata.left_queue.peek();
				pdata.cost = score_traits.compute_cost(pdata, *pdata.next_vertex);
			}
			else {
				pdata.next_vertex = pdata.left_queue.peek();
				pdata.cost = score_traits.compute_cost(pdata, *pdata.next_vertex);
				Vertex_data* vd = pdata.right_queue.peek();
				Number<Kernel> cost = score_traits.compute_cost(pdata, *vd);

				if (cost < pdata.cost) {
					pdata.cost = cost;
					pdata.next_vertex = vd;
				}
			}

			if (queue.contains(&pdata)) {
				queue.update(&pdata);
			}
			else {
				queue.push(&pdata);
			}
		}

		Vertex_data* findNextStep() {
			assert(graph.can_perform_operation());

			while (!queue.empty()) {
				Path_data* pdata = queue.peek();

				Vertex_data* vd = pdata->next_vertex;

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
					if (vd->left) {
						pdata->left_queue.pop();
					}
					else {
						pdata->right_queue.pop();
					}
					update(*pdata);
				}
			}

			// no valid steps exist
			return nullptr;
		}
		void performStep(Vertex_data& vd) {
			//std::cout << "performing " << *v << std::endl;

			assert(graph.can_perform_operation());

			Vertex_handle v = static_cast<Vertex_handle>(vd.vertex);
			Path_handle p = v->outgoing()->path();
			auto& pdata = data_traits.data(p);

			assert(queue.peek() == &pdata);
			assert(pdata.next_vertex == &vd);

			if (vd.left) {
				pdata.left_queue.pop();
			}
			else {
				pdata.right_queue.pop();
			}

			// remove from blocking lists and search structure
			pqt->remove(v);
			for (Vertex_data* bd : vd.blocking) {

				if (utils::listRemove(&vd, bd->blocked_by)) {
					if (bd->blocked_by.empty()) {
						Vertex_handle b = static_cast<Vertex_handle>(bd->vertex);
						if (b->outgoing()->path() != p) {
							update(b->outgoing()->path());
						}
					}
				}
			}

			score_traits.update(pdata, vd);

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

			update(pdata);
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
			for (Vertex_handle v : graph.vertices()) {
				pqt->insert(v);
			}

			for (Vertex_handle v : graph.vertices()) {
				Vertex_data vd = data_traits.data(v);
				vd.vertex = v;
				update(v, vd);
			}

			for (Path_handle p : graph.paths()) {
				Path_data pd = data_traits.data(p);
				pd.path = p;
				update(pd);
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

	namespace detail {
		template<KernelType Kernel>
		struct AreaIncreasedData {
			Number<Kernel> left_area_increased;
		};

		template<pvr::DataTraits DT>
		class PreservingScoreTraits {
		public:
			using Graph = DT::Graph;
			using Vertex_data = DT::Vertex_data;
			using Path_data = DT::Path_data;
			using Kernel = DT::Graph::Kernel;

			PreservingScoreTraits() : m_weight(1) {
			}
			PreservingScoreTraits(Number<Kernel> weight) : m_weight(weight) {
			}

			void initialize(Graph& graph, DT& data) {
				// skip
			}

			Number<Kernel> compute_cost(Vertex_data& vd) {
				auto v = static_cast<typename Graph::Vertex_handle>(vd.vertex);
				return CGAL::abs(
					CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
			}

			Number<Kernel> compute_cost(Path_data& pd, Vertex_data& vd) {
				return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
			}

			void update(Path_data& pd, Vertex_data& vd) {
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

		template <GraphType G>
		using PreservingDataTraits_Generic = pvr::GenericDataTraits<G, pvr::EmptyBase, AreaIncreasedData<typename G::Kernel>>;

		template<KernelType Kernel, bool Historic>
		using PreservingDataTraits_Dedicated = pvr::GraphDataTraits<Kernel, Historic, pvr::EmptyBase, AreaIncreasedData<Kernel>>;
	}

	template <GraphType G>
	using PreservingVisvalingamWhyatt_Generic = PathVertexRemoval<
		detail::PreservingDataTraits_Generic<G>,
		detail::PreservingScoreTraits<detail::PreservingDataTraits_Generic<G>>
	>;


	template<KernelType Kernel, bool Historic>
	using PreservingVisvalingamWhyatt_Dedicated = PathVertexRemoval<
		detail::PreservingDataTraits_Dedicated<Kernel, Historic>,
		detail::PreservingScoreTraits<detail::PreservingDataTraits_Dedicated<Kernel, Historic>>
	>;

} // namespace cartocrow::simplification

//#include "preserving_vertex_removal.hpp"