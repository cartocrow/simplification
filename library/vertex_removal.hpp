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
	bool VertexRemoval<MG, VRT>::runToComplexity(int k) {
		while (graph.getEdgeCount() > k) {
			if (!step()) {
				return false;
			}
		}
		return true;
	}

	template <class MG, class VRT> requires detail::VRSetup<MG, VRT>
	bool VertexRemoval<MG, VRT>::step() {
		if constexpr (ModifiableGraphWithHistory<MG>) {
			assert(graph.atPresent());
		}

		while (!queue.empty()) {
			Vertex* v = queue.pop();

			//std::cout << "trying " << v->getPoint();

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

				//std::cout << " -> removing!\n";
				// not blocked, executing!

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

				return true;
			}
			else {
				//std::cout << " -> blocked by " << v->data().blocked_by.size() << " vertices\n";
			}
		}

		return false;
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
			queue.remove(u);
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