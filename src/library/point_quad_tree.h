#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow {

template <typename P, typename K> concept PointConvertable = requires(P p) {

	{
		p.getPoint()
	} -> std::convertible_to<Point<K>&>;
};

template <typename P, typename K> requires PointConvertable<P, K> class PQTNode {
  public:
	PQTNode<P, K>* parent = nullptr;
	PQTNode<P, K>* lt = nullptr;
	PQTNode<P, K>* lb = nullptr;
	PQTNode<P, K>* rt = nullptr;
	PQTNode<P, K>* rb = nullptr;
	Rectangle<K>* rect = nullptr;
	std::vector<P*>* elts = nullptr;

	PQTNode() {}

	~PQTNode() {
		delete rect;
		delete lt;
		delete lb;
		delete rt;
		delete rb;
		delete elts;
	}
};

template <typename P, typename K> requires PointConvertable<P, K> class PointQuadTree {

  private:
	PQTNode<P, K>* root;
	int maxdepth;

	bool encloses(Rectangle<K>& larger, Rectangle<K>& smaller) {
		return larger.xmin() <= smaller.xmin() && larger.ymin() <= smaller.ymin() &&
		       larger.xmax() >= smaller.xmax() && larger.ymax() >= smaller.ymax();
	}

	bool disjoint(Rectangle<K>& a, Rectangle<K>& b) {
		return a.xmax() < b.xmin() || a.xmin() > b.xmax() || a.ymax() < b.ymin() ||
		       a.ymin() > b.ymax();
	}

	bool contains(Rectangle<K>& a, Point<K>& pt) {
		return a.xmin() <= pt.x() && pt.x() <= a.xmax() && a.ymin() <= pt.y() && pt.y() <= a.ymax();
	}

	void findContainedRecursive(PQTNode<P, K>* n, Rectangle<K>& query, std::function<void(P&)> act) {

		if (n == nullptr) {
			return;
		} else if (disjoint(*(n->rect), query)) {
			return;
		}

		if (n->elts == nullptr) {
			// internal node
			findContainedRecursive(n->lb, query, act);
			findContainedRecursive(n->lt, query, act);
			findContainedRecursive(n->rb, query, act);
			findContainedRecursive(n->rt, query, act);
		} else if (encloses(query, *(n->rect))) {
			for (P* elt : *(n->elts)) {
				act(*elt);
			}
		} else {
			for (P* elt : *(n->elts)) {
				if (contains(query, elt->getPoint())) {
					act(*elt);
				}
			}
		}
	}

	template <bool extend> PQTNode<P, K>* find(P& elt) {
		int d = 0;
		PQTNode<P, K>* n = root;
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
							n->lb = new PQTNode<P, K>();
							n->lb->rect =
							    new Rectangle<K>(n->rect->xmin(), n->rect->ymin(), xmid, ymid);
							n->lb->parent = n;
							if (d >= maxdepth) {
								n->lb->elts = new std::vector<P*>();
							}
						} else {
							return nullptr;
						}
					}
					n = n->lb;
				} else {
					// top
					if (n->lt == nullptr) {
						if constexpr (extend) {
							n->lt = new PQTNode<P, K>();
							n->lt->rect =
							    new Rectangle<K>(n->rect->xmin(), ymid, xmid, n->rect->ymax());
							n->lt->parent = n;
							if (d >= maxdepth) {
								n->lt->elts = new std::vector<P*>();
							}
						} else {
							return nullptr;
						}
					}
					n = n->lt;
				}
			} else {
				// right
				if (pt.y() <= ymid) {
					// bottom
					if (n->rb == nullptr) {
						if constexpr (extend) {
							n->rb = new PQTNode<P, K>();
							n->rb->rect =
							    new Rectangle<K>(xmid, n->rect->ymin(), n->rect->xmax(), ymid);
							n->rb->parent = n;
							if (d >= maxdepth) {
								n->rb->elts = new std::vector<P*>();
							}
						} else {
							return nullptr;
						}
					}
					n = n->rb;
				} else {
					// top
					if (n->rt == nullptr) {
						if constexpr (extend) {
							n->rt = new PQTNode<P, K>();
							n->rt->rect =
							    new Rectangle<K>(xmid, ymid, n->rect->xmax(), n->rect->ymax());
							n->rt->parent = n;
							if (d >= maxdepth) {
								n->rt->elts = new std::vector<P*>();
							}
						} else {
							return nullptr;
						}
					}
					n = n->rt;
				}
			}
		}

		return n;
	}

  public:
	PointQuadTree(Rectangle<K>& box, int depth) {
		root = new PQTNode<P, K>();
		root->rect = new Rectangle<K>(box[0], box[2]);
		maxdepth = depth;
	}

	~PointQuadTree() {
		delete root;
	}

	void insert(P& elt) {
		PQTNode<P, K>* n = find<true>(elt);
		n->elts->push_back(&elt);
	}

	bool remove(P& elt) {
		PQTNode<P, K>* n = find<false>(elt);

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

		if (n->elts->empty()) {
			do {
				PQTNode<P, K>* p = n->parent;
				if (p->lb == n) {
					p->lb = nullptr;
				} else if (p->lt == n) {
					p->lt = nullptr;
				} else if (p->rb == n) {
					p->rb = nullptr;
				} else if (p->rt == n) {
					p->rt = nullptr;
				}
				delete n;
				n = p;
			} while (n != root && n->lb == nullptr && n->lt == nullptr && n->rb == nullptr &&
			         n->rt == nullptr);
		}

		return true;
	}

	void findContained(Rectangle<K>& query, std::function<void(P&)> act) {
		findContainedRecursive(root, query, act);
	}
};

} // namespace cartocrow