// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"

namespace cartocrow::simplification {

	namespace detail {
		template <class E, typename K> struct ECBase {

			// collapse specification
			bool erase_both; // special case: both endpoints are to be removed
			Point<K> point; // general case: endpoints merge onto this point
			bool creates_difference; // special case: when the edge is collinear with its neighbors, there are no difference-triangles
			Triangle<K> T1, T2; // the two triangles of difference
			Number<K> cost; // the cost of the collapse

			// algorithm 
			bool blocked_by_degzero;
			std::vector<E*> blocked_by;
			std::vector<E*> blocking;
			int qid;

		};

		template <typename K> struct ECData : ECBase<typename EdgeCollapseGraph<K>::Edge, K> {

		};

		template <typename K> struct HECData : ECBase<typename HECGraph<K>::Edge, K> {
			Operation<HECGraph<K>>* hist = nullptr;
		};
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	void EdgeCollapse<MG, ECT>::update(Edge* e) {

		auto& edata = e->data();

		// clear topology
		for (Edge* b : edata.blocked_by) {
			utils::listRemove(e, b->data().blocking);
		}
		edata.blocked_by.clear();

		// last condition checks for a triangle
		if (e->getSource()->degree() != 2 || e->getTarget()->degree() != 2 ||
			e->sourceWalkNeighbor() == e->targetWalkNeighbor()) {
			queue.remove(e);
			return;
		}

		ECT::determineCollapse(e);

		if (queue.contains(e)) {
			queue.update(e);
		}
		else {
			queue.push(e);
		}
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	bool EdgeCollapse<MG, ECT>::blocks(Edge& edge, Edge* collapse) {
		Edge* prev = collapse->sourceWalk();
		Edge* next = collapse->targetWalk();

		if (&edge == collapse || &edge == prev || &edge == next) {
			// involved in collapse
			return false;
		}

		Vertex* prev_v = collapse->previous()->getSource();
		Vertex* next_v = collapse->next()->getTarget();;

		bool source_shared = edge.getSource() == prev_v || edge.getSource() == next_v;
		bool target_shared = edge.getTarget() == prev_v || edge.getTarget() == next_v;

		auto is_1 = CGAL::intersection(collapse->data().T1, edge.getSegment());
		if (is_1) {
			if (Point<Kernel>* pt = std::get_if<Point<Kernel>>(&*is_1)) {
				// make sure it's not the common point
				if (!(source_shared && *pt == edge.getSource()->getPoint()) &&
					!(target_shared && *pt == edge.getTarget()->getPoint())) {
					return true;
				}
			}
			else {
				// proper overlap, definitely blocking
				return true;
			}
		} // no intersection

		auto is_2 = CGAL::intersection(collapse->data().T2, edge.getSegment());
		if (is_2) {
			if (Point<Kernel>* pt = std::get_if<Point<Kernel>>(&*is_2)) {
				// make sure it's not the common point
				if (!(source_shared && *pt == edge.getSource()->getPoint()) &&
					!(target_shared && *pt == edge.getTarget()->getPoint())) {
					return true;
				}
			}
			else {
				// proper overlap, definitely blocking
				return true;
			}
		} // no intersection

		return false;
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	EdgeCollapse<MG, ECT>::EdgeCollapse(MG& g, SegmentQuadTree<Edge, Kernel>& sqt, PointQuadTree<Vertex, Kernel>& pqt)
		: graph(g), sqt(sqt), pqt(pqt) {
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	EdgeCollapse<MG, ECT>::~EdgeCollapse() {}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	void EdgeCollapse<MG, ECT>::initialize(bool initSQT, bool initPQT) {
		if (initSQT) {
			sqt.clear();
			for (Edge* e : graph.getEdges()) {
				sqt.insert(*e);
			}
		}

		if (initPQT) {
			pqt.clear();
			for (Vertex* v : graph.getVertices()) {
				if (v->degree() == 0) {
					pqt.insert(*v);
				}
			}
		}

		queue.clear();

		for (Edge* e : graph.getEdges()) {
			e->data().qid = -1;
			e->data().blocked_by.clear();
			e->data().blocking.clear();
			e->data().blocked_by_degzero = false;

			update(e);
		}

		assert(validateState());
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	bool EdgeCollapse<MG, ECT>::validateState() {
		bool ok = true;
		for (Edge* e : graph.getEdges()) {
			if (e->data().qid >= 0) {
				if (!queue.contains(e)) {
					std::cout << e << " :: thinks it's in queue but isn't\n";
					ok = false;
				}
			}
			else if (queue.contains(e)) {
				std::cout << e << " :: thinks it's not in queue, but is\n";
				ok = false;
			}

			for (Edge* b : e->data().blocked_by) {
				if (std::find(b->data().blocking.begin(), b->data().blocking.end(), e) == b->data().blocking.end()) {
					std::cout << e << " :: thinks it's blocked by " << b << ", but they don't agree\n";
					ok = false;
				}
			}

			for (Edge* b : e->data().blocking) {
				if (std::find(b->data().blocked_by.begin(), b->data().blocked_by.end(), e) == b->data().blocked_by.end()) {
					std::cout << e << " :: thinks it's blocking " << b << ", but they don't agree\n";
					ok = false;
				}
			}
		}
		return ok;
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	bool EdgeCollapse<MG, ECT>::runToComplexity(int k) {
		while (graph.getEdgeCount() > k) {
			assert(validateState());
			if (!step()) {
				return false;
			}

			// useful for debugging: stop when an invalid state is reached, without erroring
			//if (!validateState()) {
			//	return false;
			//}
		}
		return true;
	}

	template <class MG, class ECT> requires detail::ECSetup<MG, ECT>
	bool EdgeCollapse<MG, ECT>::step() {
		if constexpr (ModifiableGraphWithHistory<MG>) {
			assert(graph.atPresent());
		}

		while (!queue.empty()) {
			Edge* e = queue.pop();

			auto& edata = e->data();

			if (edata.creates_difference) {
				// possibly blocked?

				Rectangle<Kernel> rect = utils::boxOf(edata.T1, edata.T2);

				edata.blocked_by_degzero = false;

				pqt.findContained(rect, [&edata](Vertex& b) {
					if (!edata.T1.has_on_unbounded_side(b.getPoint()) ||
						!edata.T2.has_on_unbounded_side(b.getPoint())) {
						// blocked, by an unmovable vertex
						edata.blocked_by_degzero = true;
					}
					});

				if (!edata.blocked_by_degzero) {

					sqt.findOverlapped(rect, [this, &e](Edge& b) {
						if (blocks(b, e)) {
							b.data().blocking.push_back(e);
							e->data().blocked_by.push_back(&b);
						}
						});
				}

			} // else: no difference, cannot be blocked

			if (!edata.blocked_by_degzero && edata.blocked_by.empty()) {

				// not blocked, executing!

				// remove from blocking lists and search structure
				Edge* prev = e->previous();
				Edge* next = e->next();
				sqt.remove(*e);
				sqt.remove(*prev);
				sqt.remove(*next);

				queue.remove(prev);
				queue.remove(next);

				for (Edge* b : edata.blocking) {
					if (utils::listRemove(e, b->data().blocked_by)) {
						if (b->data().blocked_by.empty() && !b->data().blocked_by_degzero) {
							queue.push(b);
						}
					}
				}
				edata.blocking.clear();

				for (Edge* b : prev->data().blocking) {
					if (utils::listRemove(prev, b->data().blocked_by)) {
						if (b->data().blocked_by.empty() && !b->data().blocked_by_degzero) {
							queue.push(b);
						}
					}
				}
				prev->data().blocking.clear();

				for (Edge* b : next->data().blocking) {
					if (utils::listRemove(next, b->data().blocked_by)) {
						if (b->data().blocked_by.empty() && !b->data().blocked_by_degzero) {
							queue.push(b);
						}
					}
				}
				next->data().blocking.clear();

				if constexpr (ModifiableGraphWithHistory<MG>) {
					graph.startBatch(edata.cost);
				}

				Vertex* src = e->getSource();
				Vertex* tar = e->getTarget();

				if (edata.erase_both) {

					graph.mergeVertex(src);
					Edge* ne = graph.mergeVertex(tar);

					// insert the one new edge
					sqt.insert(*ne);

					// update it and its neighbors, if applicable
					update(ne);
					if (ne->getSource()->degree() == 2) {
						update(ne->previous());
					}
					if (ne->getTarget()->degree() == 2) {
						update(ne->next());
					}
				}
				else {

					// NB: edata will be erased on removing vertex b, hence, we need a local copy
					Point<Kernel> pt = edata.point;

					// perform the collapse
					graph.mergeVertex(src);
					graph.shiftVertex(tar, pt);

					// insert the two new edges
					sqt.insert(*tar->incoming());
					sqt.insert(*tar->outgoing());

					// update them and their neighbors, if applicable
					update(tar->incoming());
					update(tar->outgoing());

					if (tar->previous()->degree() == 2) {
						update(tar->previous()->incoming());
					}
					if (tar->next()->degree() == 2) {
						update(tar->next()->outgoing());
					}
				}

				if constexpr (ModifiableGraphWithHistory<MG>) {
					graph.endBatch();
				}

				return true;
			}
		}

		return false;
	}

	template <typename G>
	void KronenfeldEtAlTraits<G>::determineCollapse(typename G::Edge* e) {
		auto& edata = e->data(); // NB: this needs to be the handle, otherwise it doesn't update... (which is fun, because data() already returns a handle...)

		Point<Kernel> a = e->previous()->getSource()->getPoint();
		Point<Kernel> b = e->getSource()->getPoint();
		Point<Kernel> c = e->getTarget()->getPoint();
		Point<Kernel> d = e->next()->getTarget()->getPoint();

		bool abc = CGAL::collinear(a, b, c);
		bool bcd = CGAL::collinear(b, c, d);
		if (abc && bcd) {
			edata.erase_both = true;
			edata.creates_difference = false;
			edata.cost = 0;
			return;
		}
		else if (abc) {
			edata.erase_both = false;
			edata.creates_difference = false;
			edata.cost = 0;
			edata.point = c;
			return;
		}
		else if (bcd) {
			edata.erase_both = false;
			edata.creates_difference = false;
			edata.cost = 0;
			edata.point = b;
			return;
		}

		// else, no consecutive collinear edges
		edata.creates_difference = true;

		Polygon<Kernel> P;
		P.push_back(a);
		P.push_back(b);
		P.push_back(c);
		P.push_back(d);

		Line<Kernel> ad(a, d);
		Line<Kernel> ab(a, b);
		Line<Kernel> bc(b, c);
		Line<Kernel> cd(c, d);

		// area = base * height / 2
		// height = 2*area / base
		// so, we're going to rotate the vector d-a, such that we get a normal of length |d-a| = base.
		// To get a vector of length height, we then multiply this vector with height_times_base / base^2.
		// This normalized the vector and makes it length height! (without squareroots...)
		Number<Kernel> height_times_base = 2 * P.area();

		Vector<Kernel> perpv = (d - a).perpendicular(CGAL::CLOCKWISE);

		CGAL::Aff_transformation_2<Kernel> s(CGAL::SCALING, height_times_base / perpv.squared_length());
		perpv = perpv.transform(s);

		CGAL::Aff_transformation_2<Kernel> t(CGAL::TRANSLATION, perpv);
		Line<Kernel> arealine = ad.transform(t);

		if (ad.has_on_boundary(arealine.point())) {

			// these should be caught already by the collinearity checks earlier
			assert(!ad.has_on_boundary(b));
			assert(!ad.has_on_boundary(c));

			edata.erase_both = true;

			// implies that neither b nor c is on ad
			auto intersection = CGAL::intersection(bc, ad);
			Point<Kernel> pt = std::get<Point<Kernel>>(*intersection);

			edata.T1 = Triangle<Kernel>(a, b, pt);
			edata.T2 = Triangle<Kernel>(c, d, pt);

		}
		else {
			edata.erase_both = false;

			bool ab_determines_shape;
			// determine type
			if (ad.has_on_positive_side(b) == ad.has_on_positive_side(c)) {
				// same side of ab, so further point determines
				ab_determines_shape = CGAL::squared_distance(b, ad) > CGAL::squared_distance(c, ad);
			}
			else {
				// opposite sides of ad, so the one that is on same side as area line
				ab_determines_shape =
					ad.has_on_positive_side(b) == ad.has_on_positive_side(arealine.point());
			}

			// configure type
			if (ab_determines_shape) {

				auto intersection = CGAL::intersection(arealine, ab);
				edata.point = std::get<Point<Kernel>>(*intersection);

				Segment<Kernel> ns = Segment<Kernel>(edata.point, d);
				auto intersection2 = CGAL::intersection(bc, ns);
				Point<Kernel> is = std::get<Point<Kernel>>(*intersection2);

				edata.T1 = Triangle<Kernel>(b, is, edata.point);
				edata.T2 = Triangle<Kernel>(c, d, is);
			}
			else {
				auto intersection = CGAL::intersection(arealine, cd);
				edata.point = std::get<Point<Kernel>>(*intersection);

				Segment<Kernel> ns = Segment<Kernel>(edata.point, a);
				auto intersection2 = CGAL::intersection(bc, ns);
				Point<Kernel> is = std::get<Point<Kernel>>(*intersection2);

				edata.T1 = Triangle<Kernel>(a, b, is);
				edata.T2 = Triangle<Kernel>(c, is, edata.point);
			}
		}

		// since it is an area preserving method, T1 and T2 have the same area
		edata.cost = 2 * CGAL::abs(edata.T1.area());
	}
} // namespace cartocrow::simplification