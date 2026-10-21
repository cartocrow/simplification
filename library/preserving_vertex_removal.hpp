// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include <cartocrow/data_structures/graph_map_2.h>
#include "utils.h"

namespace cartocrow::simplification {

	namespace detail {

		template<class G>
		struct PVRData {

			using Graph = G;
			using Kernel = Graph::Kernel;
			using Vertex_handle = Graph::Vertex_handle;

			Number<Kernel> cost;
			bool left;
			std::vector<Vertex_handle> blocked_by;
			std::vector<Vertex_handle> blocking;
			int queue_index = -1;
		};

		template<class G, class Elt, class Map>
		class PVRQueueTraits {
		public:
			using Graph = G;
			using Kernel = Graph::Kernel;
			using Element_handle = Elt;

			PVRQueueTraits() {}
			PVRQueueTraits(Map* map) : m_map(map) {}

			void setIndex(Element_handle elt, int index) {
				(*m_map)[elt].queue_index = index;
			}

			int getIndex(Element_handle elt) const {
				return (*m_map)[elt].queue_index;
			}

			int compare(Element_handle a, Element_handle b) const {
				Number<Kernel> ac = (*m_map)[a].cost;
				Number<Kernel> bc = (*m_map)[b].cost;
				if (ac < bc) {
					return -1;
				}
				else if (ac > bc) {
					return 1;
				}
				else {
					return 0;
				}
			}

		private:
			Map* m_map = nullptr;
		};

		template<class G>
		class PVRVertexQueueTraits : public std::conditional<
			PVRGraph<G>,
			GraphQueueTraits<typename G::Vertex_handle>,
			PVRQueueTraits<G, typename G::Vertex_handle, Graph_vertex_map<G, PVRData<G>>>
		>::type
		{
		};

		template<class G>
		class PVRPathQueueTraits : public std::conditional<
			PVRGraph<G>,
			GraphQueueTraits<typename G::Path_handle>,
			PVRQueueTraits<G, typename G::Path_handle, Graph_path_map<G, PVRPathData<G>>>
		>::type
		{
		};

		template<class G>
		struct PVRPathData {

			using Graph = G;
			using Kernel = Graph::Kernel;
			using Vertex_handle = Graph::Vertex_handle;
			using VertexData = PVRData<Graph>;
			using QueueTraits = PVRVertexQueueTraits<Graph>;
			using Queue = cartocrow::data_structures::IndexedPriorityQueue<PVRVertexQueueTraits<Graph>>;

			void resetQueues(Graph_vertex_map<G, VertexData>* map) requires !PVRGraph<G> {
				left_queue = Queue(QueueTraits(map));
				right_queue = Queue(QueueTraits(map));
			}

			Number<Kernel> cost;
			Vertex_handle next_vertex;
			Number<Kernel> left_area_increased = 0;
			Queue left_queue, right_queue;
			int queue_index = -1;
		};

		template<class G>
		class PVRTraitsDataBase {
		public:
			using Graph = G;
			using Vertex_handle = Graph::Vertex_handle;
			using Path_handle = Graph::Path_handle;
			using VertexData = PVRData<G>;
			using PathData = PVRPathData<G>;

			virtual void initialize(Graph& graph) {
				// skip
			}

			static VertexData& data(Vertex_handle v) {
				return v->data();
			}

			static PathData& data(Path_handle p) {
				return p->data();
			}
		};

		template<class G>
		class PVRTraitsGenericBase {

		public:
			using Graph = G;
			using Vertex_handle = Graph::Vertex_handle;
			using Path_handle = Graph::Path_handle;
			using VertexData = PVRData<G>;
			using PathData = PVRPathData<G>;

			virtual void initialize(Graph& graph) {
				delete m_vertex_data;
				delete m_path_data;
				m_vertex_data = new Graph_vertex_map<G, VertexData>(graph, VertexData());
				m_path_data = new Graph_path_map<G, PathData>(graph, PathData());
			}

			~PVRTraitsGenericBase() {
				delete m_vertex_data;
				delete m_path_data;
			}

			VertexData& data(Vertex_handle v) {
				return (*m_vertex_data)[v];
			}

			PathData& data(Path_handle p) {
				return (*m_path_data)[p];
			}
			//private:
			Graph_vertex_map<G, VertexData>* m_vertex_data = nullptr;
			Graph_path_map<G, PathData>* m_path_data = nullptr;
		};
	}

	template <detail::PVRTraits PVRT>
	PreservingVertexRemoval<PVRT>::PreservingVertexRemoval(Graph& g, VertexTree& qt) requires std::is_default_constructible<PVRT>::value : graph(g), pqt(qt) {
	}

	template <detail::PVRTraits PVRT>
	PreservingVertexRemoval<PVRT>::PreservingVertexRemoval(Graph& g, VertexTree& qt, PVRT traits) : graph(g), pqt(qt), traits(std::move(traits)) {
	}

	template <detail::PVRTraits PVRT>
	void PreservingVertexRemoval<PVRT>::initialize(bool initQuadTree) {

		traits.initialize(graph);
		if constexpr (!detail::PVRGraph<Graph>) {
			queue = Queue(detail::PVRPathQueueTraits<Graph>(traits.m_path_data));
			for (Path_handle p : graph.paths()) {
				traits.data(p).resetQueues(traits.m_vertex_data);
			}
		}

		if (initQuadTree) {
			pqt.clear();
			for (Vertex_handle v : graph.vertices()) {
				pqt.insert(v);
			}
		}

		for (Vertex_handle v : graph.vertices()) {
			update(v);
		}

		for (Path_handle p : graph.paths()) {
			update(p);
		}
	}

	template <detail::PVRTraits PVRT>
	bool PreservingVertexRemoval<PVRT>::run(std::optional<std::function<bool(int, Number<Kernel>)>> stop) {
		while (true) {
			Vertex_handle next = findNextStep();
			if (next == nullptr) {
				return false;
			}

			if (!stop.has_value() || (*stop)(graph.number_of_edges(), traits.data(next).cost)) {
				return true;
			}

			performStep(next);
		}
	}

	template <detail::PVRTraits PVRT>
	PVRT::Graph::Vertex_handle PreservingVertexRemoval<PVRT>::findNextStep() {

		assert(graph.can_perform_operation());

		while (!queue.empty()) {
			Path_handle p = queue.peek();

			auto& pdata = traits.data(p);

			Vertex_handle v = pdata.next_vertex;

			Vertex_handle u = v->prev();
			Vertex_handle w = v->next();

			// test whether the operation is blocked
			const Point<Kernel>& up = u->point();
			const Point<Kernel>& vp = v->point();
			const Point<Kernel>& wp = w->point();
			Triangle<Kernel> T(up, vp, wp);

			Rectangle<Kernel> rect = utils::boxOf(up, vp, wp);

			auto& vdata = traits.data(v);
			pqt.findContained(rect, [&T, &u, &v, &w, &vdata, this](Vertex_handle b) {
				if (b != u && b != v && b != w && !T.has_on_unbounded_side(b->point())) {
					// blocked, record the pair
					traits.data(b).blocking.push_back(v);
					vdata.blocked_by.push_back(b);
				}
				});


			if (vdata.blocked_by.empty()) {
				// not blocked, this is the next step
				return v;
			}
			else {
				// remove the element from the queue as it's not valid and continue searching
				if (vdata.left) {
					pdata.left_queue.pop();
				}
				else {
					pdata.right_queue.pop();
				}
				update(p);
			}
		}

		// no valid steps exist
		return nullptr;
	}

	template <detail::PVRTraits PVRT>
	void PreservingVertexRemoval<PVRT>::performStep(Vertex_handle v) {

		//std::cout << "performing " << *v << std::endl;

		assert(graph.can_perform_operation());

		Path_handle p = v->outgoing()->path();
		auto& pdata = traits.data(p);
		auto& vdata = traits.data(v);

		assert(queue.peek() == p);
		assert(pdata.next_vertex == v);

		if (vdata.left) {
			pdata.left_queue.pop();
		}
		else {
			pdata.right_queue.pop();
		}

		// remove from blocking lists and search structure
		pqt.remove(v);
		for (Vertex_handle b : vdata.blocking) {

			auto& bdata = traits.data(b);
			if (utils::listRemove(v, bdata.blocked_by)) {
				if (bdata.blocked_by.empty()) {
					if (b->outgoing()->path() != p) {
						update(b->outgoing()->path());
					}
				}
			}
		}

		// perform the removal
		Vertex_handle u = v->prev();
		Vertex_handle w = v->next();

		pdata.left_area_increased -= CGAL::area(u->point(), v->point(), w->point()); // NB: signed area so positive if v is a left turn, so it decreased the area 

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

		update(p);
	}

	template <detail::PVRTraits PVRT>
	bool PreservingVertexRemoval<PVRT>::step() {

		assert(graph.can_perform_operation());

		Vertex_handle v = findNextStep();
		if (v == nullptr) {
			return false;
		}

		performStep(v);
		return true;
	}

	template <detail::PVRTraits PVRT>
	void PreservingVertexRemoval<PVRT>::update(Vertex_handle v) {
		if (v->degree() != 2) {
			return;
		}

		auto& vdata = traits.data(v);
		// clear topology
		for (Vertex_handle b : vdata.blocked_by) {
			utils::listRemove(v, traits.data(b).blocking);
		}
		vdata.blocked_by.clear();

		Vertex_handle u = v->prev();
		Vertex_handle w = v->next();

		Path_handle p = v->outgoing()->path();
		auto& pdata = traits.data(p);

		if (u->is_neighbor_of(w)) {
			if (vdata.left) {
				pdata.left_queue.remove(v);
			}
			else {
				pdata.right_queue.remove(v);
			}
			return;
		}

		vdata.cost = traits.compute_cost(v);

		bool left = CGAL::left_turn(u->point(), v->point(), w->point());;
		if (left != vdata.left) {
			vdata.left = left;
			if (vdata.left) {
				pdata.right_queue.remove(v);
				pdata.left_queue.push(v);
			}
			else {
				pdata.left_queue.remove(v);
				pdata.right_queue.push(v);
			}
		}
		else if (vdata.left) {
			if (pdata.left_queue.contains(v)) {
				pdata.left_queue.update(v);
			}
			else {
				pdata.left_queue.push(v);
			}
		}
		else {
			if (pdata.right_queue.contains(v)) {
				pdata.right_queue.update(v);
			}
			else {
				pdata.right_queue.push(v);
			}
		}
	}

	template <detail::PVRTraits PVRT>
	void PreservingVertexRemoval<PVRT>::update(Path_handle p) {
		auto& pdata = traits.data(p);

		if (pdata.left_queue.empty() && pdata.right_queue.empty()) {
			// no operation
			queue.remove(p);
			return;
		}

		if (pdata.left_queue.empty()) {
			pdata.next_vertex = pdata.right_queue.peek();
			pdata.cost = traits.compute_cost(p, pdata.next_vertex);
			//std::cout << "right only: " << *pdata.next_vertex << std::endl;
		}
		else if (pdata.right_queue.empty()) {
			pdata.next_vertex = pdata.left_queue.peek();
			pdata.cost = traits.compute_cost(p, pdata.next_vertex);
			//std::cout << "left only: " << *pdata.next_vertex << std::endl;
		}
		else {
			pdata.next_vertex = pdata.left_queue.peek();

			pdata.cost = traits.compute_cost(p, pdata.next_vertex);
			Vertex_handle v = pdata.right_queue.peek();
			Number<Kernel> cost = traits.compute_cost(p, v);

			//std::cout << "left_area_increased = " << pdata.left_area_increased << std::endl;
			//std::cout << "left candidate: " << *pdata.next_vertex << " cost = " << pdata.cost  << std::endl;
			//std::cout << "right candidate: " << *v << " cost = " << cost << std::endl;
			if (cost < pdata.cost) {
				//std::cout << "right cheaper" << std::endl;
				pdata.cost = cost;
				pdata.next_vertex = v;
			}
			else {
				//std::cout << "left cheaper" << std::endl;
			}
		}

		if (queue.contains(p)) {
			queue.update(p);
		}
		else {
			queue.push(p);
		}
	}
}