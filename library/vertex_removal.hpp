// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"

namespace cartocrow::simplification {

	namespace detail {
		template <class V, typename K>
		struct VRBase {
			Number<K> cost;
			std::vector<V*> blocked_by;
			std::vector<V*> blocking;
			int qid;
		};

		template <typename K> struct VRData : VRBase<typename VertexRemovalGraph<K>::Vertex, K> {
		};


		template <typename K> struct HVRData : VRBase<typename HVRGraph<K>::Vertex, K> {
		};

		template <typename K> struct HVREdge {
			Operation<HVRGraph<K>>* hist = nullptr;
		};
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	void VertexRemoval<MG, VRT>::initialize(bool initQuadTree) {

		if (initQuadTree) {
			pqt.clear();
			for (Vertex* v : graph.getVertices()) {
				pqt.insert(*v);
			}
		}

		for (Vertex* v : graph.getVertices()) {
			update(v);
		}
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	bool VertexRemoval<MG, VRT>::run(std::optional<std::function<bool(int, Number<Kernel>)>> stop) {
		while (true) {
			Vertex* next = findNextStep();
			if (next == nullptr) {
				return false;
			}

			if (!stop.has_value() || (*stop)(graph.getEdgeCount(), next->data().cost)) {
				return true;
			}

			performStep(next);
		}
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	MG::Vertex* VertexRemoval<MG, VRT>::findNextStep() {

		while (!queue.empty()) {
			Vertex* v = queue.peek();

			Vertex* u = v->previous();
			Vertex* w = v->next();

			// test whether the operation is blocked
			Point<Kernel>& up = u->getPoint();
			Point<Kernel>& vp = v->getPoint();
			Point<Kernel>& wp = w->getPoint();
			Triangle<Kernel> T(up, vp, wp);

			Rectangle<Kernel> rect = utils::boxOf(up, vp, wp);

			pqt.findContained(rect, [&T, &u, &v, &w](Vertex& b) {
				if (&b != u && &b != v && &b != w && !T.has_on_unbounded_side(b.getPoint())) {
					// blocked, record the pair
					b.data().blocking.push_back(v);
					v->data().blocked_by.push_back(&b);
				}
				});

			if (v->data().blocked_by.empty()) {
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

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	void VertexRemoval<MG, VRT>::performStep(Vertex* v) {

		assert(queue.peek() == v);

		queue.pop();

		// remove from blocking lists and search structure
		pqt.remove(*v);
		for (Vertex* b : v->data().blocking) {

			if (utils::listRemove(v, b->data().blocked_by)) {
				if (b->data().blocked_by.empty()) {
					queue.push(b);
				}
			}
		}

		// perform the removal
		Vertex* u = v->previous();
		Vertex* w = v->next();

		if constexpr (ModifiableGraphWithHistory<MG>) {
			graph.startBatch(v->data().cost);
		}

		graph.mergeVertex(v);

		if constexpr (ModifiableGraphWithHistory<MG>) {
			graph.endBatch();
		}

		// update the neighbors
		update(u);
		update(w);

		// and their common neighbors (to avoid issues with triangles collapsing)
		for (int i = 0; i < u->degree(); i++) {
			Vertex* nbr = u->neighbor(i);
			if (nbr->isNeighborOf(w)) {
				update(nbr);
			}
		}
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	bool VertexRemoval<MG, VRT>::step() {
		if constexpr (ModifiableGraphWithHistory<MG>) {
			assert(graph.atPresent());
		}

		Vertex* v = findNextStep();
		if (v == nullptr) {
			return false;
		}

		performStep(v);
		return true;
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	void VertexRemoval<MG, VRT>::update(Vertex* v) {
		if (v->degree() != 2) {
			return;
		}

		// clear topology
		for (Vertex* b : v->data().blocked_by) {
			utils::listRemove(v, b->data().blocking);
		}
		v->data().blocked_by.clear();

		Vertex* u = v->previous();
		Vertex* w = v->next();

		if (u->isNeighborOf(w)) {
			queue.remove(v);
			return;
		}

		v->data().cost = VRT::getCost(v);

		if (queue.contains(v)) {
			queue.update(v);
		}
		else {
			queue.push(v);
		}
	}

	template <typename G>
	Number<typename G::Kernel> VisvalingamWhyattTraits<G>::getCost(typename G::Vertex* v) {
		return CGAL::abs(
			CGAL::area(v->getPoint(), v->previous()->getPoint(), v->next()->getPoint()));
	}
}