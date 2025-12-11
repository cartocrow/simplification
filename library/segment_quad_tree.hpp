// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"

namespace cartocrow::simplification {

	namespace detail {
		template <typename P, typename K> requires SegmentConvertable<P, K> struct SQTNode {

			using Node = SQTNode<P, K>;

			Node* parent;
			Node* lt = nullptr;
			Node* lb = nullptr;
			Node* rt = nullptr;
			Node* rb = nullptr;
			Rectangle<K>* rect;
			bool inf_left, inf_right, inf_bottom, inf_top;
			std::vector<P*>* elts;

			SQTNode(SQTNode<P, K>* parent, Rectangle<K>* rect, bool inf_left, bool inf_right,
				bool inf_bottom, bool inf_top)
				: parent(parent), rect(rect), inf_left(inf_left), inf_right(inf_right),
				inf_bottom(inf_bottom), inf_top(inf_top) {
				elts = new std::vector<P*>();
			}

			~SQTNode() {
				delete rect;
				delete lt;
				delete lb;
				delete rt;
				delete rb;
				delete elts;
			}
		};
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	bool SegmentQuadTree<P, K>::encloses(Rectangle<K>& rect, Node* node) {
		// node extends further to the left
		if (node->inf_left || rect.xmin() > node->rect->xmin()) {
			return false;
		}

		// node extends further to the right
		if (node->inf_right || rect.xmax() < node->rect->xmax()) {
			return false;
		}

		// node extends further to the bottom
		if (node->inf_bottom || rect.ymin() > node->rect->ymin()) {
			return false;
		}

		// node extends further to the top
		if (node->inf_top || rect.ymax() < node->rect->ymax()) {
			return false;
		}

		return true;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	bool SegmentQuadTree<P, K>::disjoint(Node* node, Rectangle<K>& rect) {
		// rect left of node
		if (!node->inf_left && rect.xmax() < node->rect->xmin()) {
			return true;
		}

		// rect right of node
		if (!node->inf_right && rect.xmin() > node->rect->xmax()) {
			return true;
		}

		// rect below node
		if (!node->inf_bottom && rect.ymax() < node->rect->ymin()) {
			return true;
		}

		// rect above node
		if (!node->inf_top && rect.ymin() > node->rect->ymax()) {
			return true;
		}

		return false;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	void SegmentQuadTree<P, K>::findOverlappedRecursive(Node* n, Rectangle<K>& query, std::function<void(P&)> act) {

		if (n == nullptr) {
			return;
		}
		else if (disjoint(n, query)) {
			return;
		}

		if (encloses(query, n)) {
			for (P* elt : *(n->elts)) {
				act(*elt);
			}
		}
		else {
			for (P* elt : *(n->elts)) {
				Segment<K> seg = elt->getSegment();
				if (utils::overlaps(query, seg)) {
					act(*elt);
				}
			}
		}

		findOverlappedRecursive(n->lb, query, act);
		findOverlappedRecursive(n->lt, query, act);
		findOverlappedRecursive(n->rb, query, act);
		findOverlappedRecursive(n->rt, query, act);
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	template <bool extend>
	detail::SQTNode<P, K>* SegmentQuadTree<P, K>::find(P& elt) {
		int d = 0;
		Node* n = root;
		Segment<K> seg = elt.getSegment();

		Number<K> seg_left = CGAL::min(seg[0].x(), seg[1].x());
		Number<K> seg_right = CGAL::max(seg[0].x(), seg[1].x());
		Number<K> seg_bottom = CGAL::min(seg[0].y(), seg[1].y());
		Number<K> seg_top = CGAL::max(seg[0].y(), seg[1].y());

		// NB: extending, n will never become null
		while (d < maxdepth) {
			d++;

			Number<K> xmid = (n->rect->xmin() + n->rect->xmax()) / 2;
			Number<K> ymid = (n->rect->ymin() + n->rect->ymax()) / 2;
			Number<K> fx = (n->rect->xmax() - n->rect->xmin()) / 2.0 * fuzziness;
			Number<K> fy = (n->rect->ymax() - n->rect->ymin()) / 2.0 * fuzziness;

			if (seg_right <= xmid + fx) {
				// left
				if (seg_top <= ymid + fy) {
					// bottom
					if (n->lb == nullptr) {
						if constexpr (extend) {
							n->lb =
								new Node(n,
									new Rectangle<K>(n->rect->xmin(), n->rect->ymin(),
										xmid + fx, ymid + fx),
									n->inf_left, false, n->inf_bottom, false);
						}
						else {
							return nullptr;
						}
					}
					n = n->lb;
				}
				else if (seg_bottom >= ymid - fy) {
					// top
					if (n->lt == nullptr) {
						if constexpr (extend) {
							n->lt = new Node(n,
								new Rectangle<K>(n->rect->xmin(), ymid - fy,
									xmid + fx, n->rect->ymax()),
								n->inf_left, false, false, n->inf_top);
						}
						else {
							return nullptr;
						}
					}
					n = n->lt;
				}
				else {
					// here
					return n;
				}
			}
			else if (seg_left >= xmid - fx) {
				// right
				if (seg_top <= ymid + fy) {
					// bottom
					if (n->rb == nullptr) {
						if constexpr (extend) {
							n->rb = new Node(n,
								new Rectangle<K>(xmid - fx, n->rect->ymin(),
									n->rect->xmax(), ymid + fy),
								false, n->inf_right, n->inf_bottom, false);
						}
						else {
							return nullptr;
						}
					}
					n = n->rb;
				}
				else if (seg_bottom >= ymid - fy) {
					// top
					if (n->rt == nullptr) {
						if constexpr (extend) {
							n->rt =
								new Node(n,
									new Rectangle<K>(xmid - fx, ymid - fy,
										n->rect->xmax(), n->rect->ymax()),
									false, n->inf_right, false, n->inf_top);
						}
						else {
							return nullptr;
						}
					}
					n = n->rt;
				}
				else {
					// here
					return n;
				}
			}
			else {
				// here
				return n;
			}
		}

		return n;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	SegmentQuadTree<P, K>::SegmentQuadTree(Rectangle<K>& box, int depth, Number<K> fuzz) {
		root = new Node(nullptr, new Rectangle<K>(box[0], box[2]), true, true, true, true);
		maxdepth = depth;
		fuzziness = fuzz;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	SegmentQuadTree<P, K>::~SegmentQuadTree() {
		delete root;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	void SegmentQuadTree<P, K>::clear() {
		// just delete all nodes except the root
		delete root->lb;
		delete root->rb;
		delete root->lt;
		delete root->rt;
		root->lb = nullptr;
		root->rb = nullptr;
		root->lt = nullptr;
		root->rt = nullptr;

		// and empty the elements at the root
		root->elts->clear();
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	void SegmentQuadTree<P, K>::insert(P& elt) {
		Node* n = find<true>(elt);
		n->elts->push_back(&elt);
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	bool SegmentQuadTree<P, K>::remove(P& elt) {
		Node* n = find<false>(elt);

		if (n == nullptr) {
			// it's supposed leaf doesn't exist
			return false;
		}

		auto position = std::find(n->elts->begin(), n->elts->end(), &elt);
		if (position == n->elts->end()) {
			// the leaf doesn't actually contain the element
			return false;
		}

		// it's contained: erase and clean up the tree if possible
		n->elts->erase(position);

		while (n != root && n->elts->empty() && n->lb == nullptr && n->lt == nullptr &&
			n->rb == nullptr && n->rt == nullptr) {
			Node* p = n->parent;
			if (p->lb == n) {
				p->lb = nullptr;
			}
			else if (p->lt == n) {
				p->lt = nullptr;
			}
			else if (p->rb == n) {
				p->rb = nullptr;
			}
			else if (p->rt == n) {
				p->rt = nullptr;
			}
			delete n;
			n = p;
		}

		return true;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K>
	void SegmentQuadTree<P, K>::findOverlapped(Rectangle<K>& query, std::function<void(P&)> act) {
		findOverlappedRecursive(root, query, act);
	}

} // namespace cartocrow::simplification