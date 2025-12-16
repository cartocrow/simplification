#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	template <typename P, typename K> concept PointConvertable = requires(P p) {

		{
			p.getPoint()
		} -> std::convertible_to<Point<K>&>;
	};

	namespace detail {
		template <typename P, typename K> requires PointConvertable<P, K> struct PQTNode;
	}

	template <typename P, typename K> requires PointConvertable<P, K> class PointQuadTree {

	private:
		using Node = detail::PQTNode<P, K>;

		Node* root;
		int maxdepth;

		P* findElementRecursive(Node* n, const  Point<K>& query, const Number<K> prec);
		void findContainedRecursive(Node* n, Rectangle<K>& query, std::function<void(P&)> act);
		template <bool extend>Node* find(P& elt);

	public:
		PointQuadTree(Rectangle<K>& box, int depth);
		~PointQuadTree();

		void clear();
		void insert(P& elt);
		bool remove(P& elt);

		void findContained(Rectangle<K>& query, std::function<void(P&)> act);
		P* findElement(const Point<K>& query, const Number<K> prec = 0);
	};

} // namespace cartocrow::simplification

#include "point_quad_tree.hpp"