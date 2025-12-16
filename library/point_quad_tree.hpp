// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------

#include "utils.h"

namespace cartocrow::simplification {

	namespace detail {

		template <typename P, typename K> requires PointConvertable<P, K> struct PQTNode {

			using Node = PQTNode<P, K>;

			Node* parent;
			Node* lt;
			Node* lb;
			Node* rt;
			Node* rb;
			Rectangle<K>* rect;
			std::vector<P*>* elts;

			PQTNode() {
				parent = nullptr;
				lt = nullptr;
				lb = nullptr;
				rt = nullptr;
				rb = nullptr;
				rect = nullptr;
				elts = nullptr;
			}

			~PQTNode() {
				delete rect;
				delete lt;
				delete lb;
				delete rt;
				delete rb;
				delete elts;
			}
		};
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	void PointQuadTree<P, K>::findContainedRecursive(Node* n, Rectangle<K>& query, std::function<void(P&)> act) {

		if (n == nullptr) {
			return;
		}
		else if (utils::disjoint(*(n->rect), query)) {
			return;
		}

		if (n->elts == nullptr) {
			// internal node
			findContainedRecursive(n->lb, query, act);
			findContainedRecursive(n->lt, query, act);
			findContainedRecursive(n->rb, query, act);
			findContainedRecursive(n->rt, query, act);
		}
		else if (utils::encloses(query, *(n->rect))) {
			for (P* elt : *(n->elts)) {
				act(*elt);
			}
		}
		else {
			for (P* elt : *(n->elts)) {
				if (utils::contains(query, elt->getPoint())) {
					act(*elt);
				}
			}
		}
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	template<bool extend>
	detail::PQTNode<P, K>* PointQuadTree<P, K>::find(P& elt) {
		int d = 0;
		Node* n = root;
		Point<K>& pt = elt.getPoint();

		while (n->elts == nullptr) {
			d++;

			Number<K> xmid = (n->rect->xmin() + n->rect->xmax()) / 2;
			Number<K> ymid = (n->rect->ymin() + n->rect->ymax()) / 2;

			if (pt.x() <= xmid) {
				// left
				if (pt.y() <= ymid) {
					// bottom
					if (n->lb == nullptr) {
						if constexpr (extend) {
							n->lb = new Node();
							n->lb->rect =
								new Rectangle<K>(n->rect->xmin(), n->rect->ymin(), xmid, ymid);
							n->lb->parent = n;
							if (d >= maxdepth) {
								n->lb->elts = new std::vector<P*>();
							}
						}
						else {
							return nullptr;
						}
					}
					n = n->lb;
				}
				else {
					// top
					if (n->lt == nullptr) {
						if constexpr (extend) {
							n->lt = new Node();
							n->lt->rect =
								new Rectangle<K>(n->rect->xmin(), ymid, xmid, n->rect->ymax());
							n->lt->parent = n;
							if (d >= maxdepth) {
								n->lt->elts = new std::vector<P*>();
							}
						}
						else {
							return nullptr;
						}
					}
					n = n->lt;
				}
			}
			else {
				// right
				if (pt.y() <= ymid) {
					// bottom
					if (n->rb == nullptr) {
						if constexpr (extend) {
							n->rb = new Node();
							n->rb->rect =
								new Rectangle<K>(xmid, n->rect->ymin(), n->rect->xmax(), ymid);
							n->rb->parent = n;
							if (d >= maxdepth) {
								n->rb->elts = new std::vector<P*>();
							}
						}
						else {
							return nullptr;
						}
					}
					n = n->rb;
				}
				else {
					// top
					if (n->rt == nullptr) {
						if constexpr (extend) {
							n->rt = new Node();
							n->rt->rect =
								new Rectangle<K>(xmid, ymid, n->rect->xmax(), n->rect->ymax());
							n->rt->parent = n;
							if (d >= maxdepth) {
								n->rt->elts = new std::vector<P*>();
							}
						}
						else {
							return nullptr;
						}
					}
					n = n->rt;
				}
			}
		}

		return n;
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	PointQuadTree<P, K>::PointQuadTree(Rectangle<K>& box, int depth) {
		root = new Node();
		root->rect = new Rectangle<K>(box[0], box[2]);
		maxdepth = depth;
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	PointQuadTree<P, K>::~PointQuadTree() {
		delete root;
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	void PointQuadTree<P, K>::clear() {
		// just delete all nodes except the root
		delete root->lb;
		delete root->rb;
		delete root->lt;
		delete root->rt;
		root->lb = nullptr;
		root->rb = nullptr;
		root->lt = nullptr;
		root->rt = nullptr;
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	void PointQuadTree<P, K>::insert(P& elt) {
		Node* n = find<true>(elt);
		n->elts->push_back(&elt);
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	bool PointQuadTree<P, K>::remove(P& elt) {
		Node* n = find<false>(elt);

		if (n == nullptr) {
			// it's supposed leaf doesn't exist
			return false;
		}

		if (!utils::listRemove(&elt, *(n->elts))) {
			// the leaf didn't actually contain the element
			return false;
		}


		// clean up the tree if possible
		if (n->elts->empty()) {
			do {
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
			} while (n != root && n->lb == nullptr && n->lt == nullptr && n->rb == nullptr &&
				n->rt == nullptr);
		}

		return true;
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	void PointQuadTree<P, K>::findContained(Rectangle<K>& query, std::function<void(P&)> act) {
		findContainedRecursive(root, query, act);
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	P* PointQuadTree<P, K>::findElementRecursive(Node* n, const  Point<K>& query, const Number<K> prec) {
		if (n == nullptr || !utils::contains(*(n->rect), query, prec)) {
			// outside of the node's rectangle
			return nullptr;
		}
		else if (n->elts == nullptr) {
			// internal node
			P* elt = findElementRecursive(n->lb, query, prec);
			if (elt != nullptr) {
				return elt;
			}
			elt = findElementRecursive(n->lt, query, prec);
			if (elt != nullptr) {
				return elt;
			}
			elt = findElementRecursive(n->rb, query, prec);
			if (elt != nullptr) {
				return elt;
			}
			elt = findElementRecursive(n->rt, query, prec);
			return elt;
		}
		else {
			// leaf node
			for (P* elt : *(n->elts)) {
				if (utils::samePoint(elt->getPoint(), query, prec)) {
					return elt;
				}
			}
			return nullptr;
		}
	}

	template <typename P, typename K> requires PointConvertable<P, K>
	P* PointQuadTree<P, K>::findElement(const Point<K>& query, const  Number<K> prec) {
		return findElementRecursive(root, query, prec);
	}
} // namespace cartocrow::simplification