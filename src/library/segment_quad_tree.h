#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

	template <typename P, typename K> concept SegmentConvertable = requires(P p) {

		{
			p.getSegment()
		} -> std::convertible_to<Segment<K>>;
	};

	namespace detail {
		template <typename P, typename K> requires SegmentConvertable<P, K> class SQTNode;
	}

	template <typename P, typename K> requires SegmentConvertable<P, K> class SegmentQuadTree {
	private:
		using Node = detail::SQTNode<P, K>;

		Node* root;
		int maxdepth;
		Number<K> fuzziness;

		// Does the rectangle enclose the (possibly infinite) node?
		bool encloses(Rectangle<K>& rect, Node* node);

		// Is the (possibly infinite) node disjoint from the rectangle
		bool disjoint(Node* node, Rectangle<K>& rect);

		void findOverlappedRecursive(Node* n, Rectangle<K>& query, std::function<void(P&)> act);

		template <bool extend> Node* find(P& elt);

	public:
		SegmentQuadTree(Rectangle<K>& box, int depth, Number<K> fuzz);
		~SegmentQuadTree();

		void clear();
		void insert(P& elt);
		bool remove(P& elt);

		void findOverlapped(Rectangle<K>& query, std::function<void(P&)> act);
	};

} // namespace cartocrow::simplification

#include "segment_quad_tree.hpp"