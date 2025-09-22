#pragma once

#include <cartocrow/core/core.h>

#include "indexed_priority_queue.h"

namespace cartocrow::simplification {

class ECVertexData {
	// nothing to store for a vertex
};

template <typename K> class ECData;
template <typename K> using ECGraph = StraightGraph<ECVertexData, ECData<K>, K>;
template <typename K> using ECVertex = StraightVertex<ECVertexData, ECData<K>, K>;
template <typename K> using ECEdge = StraightEdge<ECVertexData, ECData<K>, K>;

template <typename K> struct Collapse {
	bool erase_both;
	Point<K> point;
	Polygon<K> T1, T2;
};

template <class ECT, typename K> concept EdgeCollapseTraits = requires(ECEdge<K> * e) {

	{ECT::determineCollapse(e)};
};

template <class ECT, typename K> requires EdgeCollapseTraits<ECT, K> class EdgeCollapse;

template <typename K> struct ECQueueTraits;

template <typename K> class ECData {
	// collapse information (public, such that custom traits can set them)
  public:
	bool erase_both; // special case: both endpoints are to be removed
	Point<K> point; // general case: endpoints merge onto this point
	bool creates_difference; // special case: when the edge is collinear with its neighbors, there are no difference-triangles
	Triangle<K> T1, T2; // the two triangles of difference
	Number<K> cost; // the cost of the collapse

  private:
	// blocking information
	bool blocked_by_degzero;
	std::vector<ECEdge<K>*> blocked_by;
	std::vector<ECEdge<K>*> blocking;

	// queue information
	int qid;

	template <class ECT, typename K> requires EdgeCollapseTraits<ECT, K> friend class EdgeCollapse;

	friend class ECQueueTraits<K>;
};

template <typename K> struct ECQueueTraits {

	static void setIndex(ECEdge<K>* elt, int index) {
		elt->data().qid = index;
	}

	static int getIndex(ECEdge<K>* elt) {
		return elt->data().qid;
	}

	static int compare(ECEdge<K>* a, ECEdge<K>* b) {
		Number<K> ac = a->data().cost;
		Number<K> bc = b->data().cost;
		if (ac < bc) {
			return -1;
		} else if (ac > bc) {
			return 1;
		} else {
			return 0;
		}
	}
};

template <class ECT, typename K> requires EdgeCollapseTraits<ECT, K> class EdgeCollapse {

  private:
	ECGraph<K>& graph;
	SegmentQuadTree<ECEdge<K>, K>& sqt;
	PointQuadTree<ECVertex<K>, K>& pqt;
	IndexedPriorityQueue<ECEdge<K>, ECQueueTraits<K>> queue;

	void update(ECEdge<K>* e) {

		// last condition checks for a triangle
		if (e->getSource()->degree() != 2 || e->getTarget()->degree() != 2 ||
		    e->sourceWalkNeighbor() == e->targetWalkNeighbor()) {
			queue.remove(e);
			return;
		}

		ECT::determineCollapse(e);

		ECData<K>& edata = e->data();

		// clear topology
		for (ECEdge<K>* b : edata.blocked_by) {
			auto position = std::find(b->data().blocking.begin(), b->data().blocking.end(), e);
			if (position != b->data().blocking.end()) {
				b->data().blocking.erase(position);
			}
		}
		edata.blocked_by.clear();

		if (queue.contains(e)) {
			queue.update(e);
		} else {
			queue.push(e);
		}
	}

	Rectangle<K> boxOf(Triangle<K>& T1, Triangle<K>& T2) {

		Number<K> left = CGAL::min(CGAL::min(T1[0].x(), CGAL::min(T1[1].x(), T1[2].x())),
		                           CGAL::min(T2[0].x(), CGAL::min(T2[1].x(), T2[2].x())));
		Number<K> right = CGAL::max(CGAL::max(T1[0].x(), CGAL::max(T1[1].x(), T1[2].x())),
		                            CGAL::max(T2[0].x(), CGAL::max(T2[1].x(), T2[2].x())));

		Number<K> bottom = CGAL::min(CGAL::min(T1[0].y(), CGAL::min(T1[1].y(), T1[2].y())),
		                             CGAL::min(T2[0].y(), CGAL::min(T2[1].y(), T2[2].y())));
		Number<K> top = CGAL::max(CGAL::max(T1[0].y(), CGAL::max(T1[1].y(), T1[2].y())),
		                          CGAL::max(T2[0].y(), CGAL::max(T2[1].y(), T2[2].y())));

		return Rectangle<K>(left, bottom, right, top);
	}

	bool blocks(ECEdge<K>& edge, ECEdge<K>* collapse) {
		ECEdge<K>* prev = collapse->sourceWalk();
		ECEdge<K>* next = collapse->targetWalk();

		if (&edge == collapse || &edge == prev || &edge == next) {
			// involved in collapse
			return false;
		}

		ECVertex<K>* prev_v = collapse->sourceWalkNeighbor();
		ECVertex<K>* next_v = collapse->targetWalkNeighbor();

		bool source_shared = edge.getSource() == prev_v || edge.getSource() == next_v;
		bool target_shared = edge.getTarget() == prev_v || edge.getTarget() == next_v;

		auto is_1 = CGAL::intersection(collapse->data().T1, edge.getSegment());
		if (is_1) {
			if (Point<K>* pt = std::get_if<Point<K>>(&*is_1)) {
				// make sure it's not the common point
				if (!(source_shared && *pt == edge.getSource()->getPoint()) &&
				    !(target_shared && *pt == edge.getTarget()->getPoint())) {
					return true;
				}
			} else {
				// proper overlap, definitely blocking
				return true;
			}
		} // no intersection

		auto is_2 = CGAL::intersection(collapse->data().T2, edge.getSegment());
		if (is_2) {
			if (Point<K>* pt = std::get_if<Point<K>>(&*is_2)) {
				// make sure it's not the common point
				if (!(source_shared && *pt == edge.getSource()->getPoint()) &&
				    !(target_shared && *pt == edge.getTarget()->getPoint())) {
					return true;
				}
			} else {
				// proper overlap, definitely blocking
				return true;
			}
		} // no intersection

		return false;
	}

  public:
	EdgeCollapse(ECGraph<K>& g, SegmentQuadTree<ECEdge<K>, K>& sqt, PointQuadTree<ECVertex<K>, K>& pqt)
	    : graph(g), sqt(sqt), pqt(pqt) {}
	~EdgeCollapse() {	}

	void initialize(bool initSQT, bool initPQT) {
		if (initSQT) {
			for (ECEdge<K>* e : graph.getEdges()) {
				sqt.insert(*e);
			}
		}

		if (initPQT) {
			for (ECVertex<K>* v : graph.getVertices()) {
				if (v->degree() == 0) {
					pqt.insert(*v);
				}
			}
		}

		for (ECEdge<K>* e : graph.getEdges()) {
			update(e);
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
		while (!queue.empty()) {
			ECEdge<K>* e = queue.pop();

			std::cout << "trying " << e->getSegment();

			ECData<K>& edata = e->data();

			if (edata.creates_difference) {
				// possibly blocked?

				Rectangle<K> rect = boxOf(edata.T1, edata.T2);

				edata.blocked_by_degzero = false;

				pqt.findContained(rect, [&edata](ECVertex<K>& b) {
					if (!edata.T1.has_on_unbounded_side(b.getPoint()) ||
					    !edata.T2.has_on_unbounded_side(b.getPoint())) {
						// blocked, by an unmovable vertex
						edata.blocked_by_degzero = true;
					}
				});

				if (!edata.blocked_by_degzero) {

					sqt.findOverlapped(rect, [this, &e](ECEdge<K>& b) {
						if (blocks(b, e)) {
							b.data().blocking.push_back(e);
							e->data().blocked_by.push_back(&b);
						}
					});
				}

			} // else: no difference, cannot be blocked

			if (!edata.blocked_by_degzero && edata.blocked_by.empty()) {

				std::cout << " -> collapsing!\n";
				// not blocked, executing!

				// remove from blocking lists and search structure
				ECEdge<K>* prev = e->sourceWalk();
				ECEdge<K>* next = e->targetWalk();
				sqt.remove(*e);
				sqt.remove(*prev);
				sqt.remove(*next);

				queue.remove(prev);
				queue.remove(next);

				for (ECEdge<K>* b : edata.blocking) {

					auto position =
					    std::find(b->data().blocked_by.begin(), b->data().blocked_by.end(), e);
					if (position != b->data().blocked_by.end()) {
						b->data().blocked_by.erase(position);
						if (b->data().blocked_by.empty() && !b->data().blocked_by_degzero) {
							queue.push(b);
						}
					}
				}

				ECVertex<K>* a = e->sourceWalkNeighbor();
				ECVertex<K>* b = e->getSource();
				ECVertex<K>* c = e->getTarget();
				ECVertex<K>* d = e->targetWalkNeighbor();

				if (edata.erase_both) {
					// perform the collapse
					graph.removeVertex(b);
					graph.removeVertex(c);
					ECEdge<K>* ne = graph.addEdge(a, b);

					// insert the one new edge
					sqt.insert(*ne);

					// update it and its neighbors, if applicable
					update(ne);
					if (ne->getSource()->degree() == 2) {
						update(ne->sourceWalk());
					}
					if (ne->getTarget()->degree() == 2) {
						update(ne->targetWalk());
					}
				} else {

					// NB: edata will be erased on removing vertex b, hence, we need a local copy
					Point<K> pt = edata.point;

					// perform the collapse
					graph.removeVertex(b);
					ECEdge<K>* ne = graph.addEdge(a, c);
					ECEdge<K>* ne2 = ne->targetWalk();

					c->setPoint(pt);

					// insert the two "new" edges
					sqt.insert(*ne);
					sqt.insert(*ne2);

					// update them and their neighbors, if applicable
					update(ne);
					update(ne2);

					if (ne->getSource()->degree() == 2) {
						update(ne->sourceWalk());
					}
					if (ne2->getTarget()->degree() == 2) {
						update(ne2->targetWalk());
					}
				}

				return true;
			} else {
				std::cout << " -> blocked by " << edata.blocked_by.size()
				          << " edges and by degzero: " << edata.blocked_by_degzero << " \n ";
			}
		}

		return false;
	}
};

} // namespace cartocrow::simplification