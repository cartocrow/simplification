#pragma once

#include <cartocrow/core/core.h>

#include "point_quad_tree.h"
#include "straight_graph.h"
#include "indexed_priority_queue.h"
#include "modifiable_graph.h"
#include "historic_graph.h"

namespace cartocrow::simplification {

	//-------------- NO HISTORY --------------------------------------------------

	template <typename K> struct VRData;

	template<typename K>
	using VertexRemovalGraph = StraightGraph<VRData<K>, VoidData, K>;

	template <typename K> struct VRData {
		Number<typename K> cost;
		std::vector<typename VertexRemovalGraph<K>::Vertex*> blocked_by;
		std::vector<typename VertexRemovalGraph<K>::Vertex*> blocking;
		int qid;
	};

	//-------------- WITH HISTORY ------------------------------------------------

	template <typename K> struct HVRData;

	template <typename K> struct HVREdge;

	template<typename K>
	using HVRGraph = StraightGraph<HVRData<K>, HVREdge<K>, K>;

	template <typename K> struct HVRData {
		Number<typename K> cost;
		std::vector<typename HVRGraph<K>::Vertex*> blocked_by;
		std::vector<typename HVRGraph<K>::Vertex*> blocking;
		int qid;
	};

	template <typename K> struct HVREdge {
		Operation<HVRGraph<K>>* hist;
	};

	template<typename K>
	using HistoricVertexRemovalGraph = HistoricGraph<HVRGraph<K>>;

	//-------------- VERTEX REMOVAL ----------------------------------------------

	template <class MG, class VRT>
	concept VRSetup = requires(MG::Vertex *v) {
		requires ModifiableGraph<MG>;

		requires std::same_as<typename MG::Kernel, typename VRT::Kernel>;
		//requires std::is_base_of<typename VRData<typename MG::Kernel>, typename MG::Edge::Data>;
		{
		VRT::getCost(v)
	} -> std::same_as<Number<typename VRT::Kernel>>;
	};

	template <class MG, class VRT>
		requires VRSetup<MG, VRT> class VertexRemoval {

		public:
			using Vertex = MG::Vertex;
			using Kernel = MG::Kernel;

		private:
			struct VRQueueTraits {

				static void setIndex(Vertex* elt, int index) {
					elt->data().qid = index;
				}

				static int getIndex(Vertex* elt) {
					return elt->data().qid;
				}

				static int compare(Vertex* a, Vertex* b) {
					Number<Kernel> ac = a->data().cost;
					Number<Kernel> bc = b->data().cost;
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
			};

			MG& graph;
			PointQuadTree<Vertex, Kernel>& pqt;
			IndexedPriorityQueue<Vertex, VRQueueTraits> queue;

			void update(Vertex* v) {
				if (v->degree() != 2) {
					return;
				}

				// clear topology
				for (Vertex* b : v->data().blocked_by) {
					auto position = std::find(b->data().blocking.begin(), b->data().blocking.end(), v);
					if (position != b->data().blocking.end()) {
						b->data().blocking.erase(position);
					}
				}
				v->data().blocked_by.clear();

				Vertex* u = v->neighbor(0);
				Vertex* w = v->neighbor(1);
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

			Rectangle<Kernel> boxOf(Point<Kernel>& a, Point<Kernel>& b, Point<Kernel>& c) {
				Number<Kernel> left = CGAL::min(a.x(), CGAL::min(b.x(), c.x()));
				Number<Kernel> right = CGAL::max(a.x(), CGAL::max(b.x(), c.x()));
				Number<Kernel> bottom = CGAL::min(a.y(), CGAL::min(b.y(), c.y()));
				Number<Kernel> top = CGAL::max(a.y(), CGAL::max(b.y(), c.y()));

				return Rectangle<Kernel>(left, bottom, right, top);
			}

		public:
			VertexRemoval(MG& g, PointQuadTree<Vertex, Kernel>& qt) : graph(g), pqt(qt) {}
			~VertexRemoval() {}

			void initialize(bool initQuadTree) {

				if (initQuadTree) {
					for (Vertex* v : graph.getVertices()) {
						pqt.insert(*v);
					}
				}

				for (Vertex* v : graph.getVertices()) {
					update(v);
				}
			}

			bool runToComplexity(int k) {
				while (graph.getEdgeCount() > k) {
					if (!step()) {
						return false;
					}
				}
				return true;
			}

			bool step() {
				if constexpr (ModifiableGraphWithHistory<MG>) {
					assert(graph.atPresent());
				}

				while (!queue.empty()) {
					Vertex* v = queue.pop();

					std::cout << "trying " << v->getPoint();

					Vertex* u = v->neighbor(0);
					Vertex* w = v->neighbor(1);

					// test whether the operation is blocked
					Point<Kernel>& up = u->getPoint();
					Point<Kernel>& vp = v->getPoint();
					Point<Kernel>& wp = w->getPoint();
					Triangle<Kernel> T(up, vp, wp);

					Rectangle<Kernel> rect = boxOf(up, vp, wp);

					pqt.findContained(rect, [&T, &u, &v, &w](Vertex& b) {
						if (&b != u && &b != v && &b != w && !T.has_on_unbounded_side(b.getPoint())) {
							// blocked, record the pair
							b.data().blocking.push_back(v);
							v->data().blocked_by.push_back(&b);
						}
						});

					if (v->data().blocked_by.empty()) {

						std::cout << " -> removing!\n";
						// not blocked, executing!

						// remove from blocking lists and search structure
						pqt.remove(*v);
						for (Vertex* b : v->data().blocking) {

							auto position =
								std::find(b->data().blocked_by.begin(), b->data().blocked_by.end(), v);
							if (position != b->data().blocked_by.end()) {
								b->data().blocked_by.erase(position);
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
						std::cout << " -> blocked by " << v->data().blocked_by.size() << " vertices\n";
					}
				}

				return false;
			}
	};

} // namespace cartocrow::simplification