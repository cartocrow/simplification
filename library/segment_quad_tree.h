#pragma once

#include <cartocrow/datastructures/quad_tree.h>

namespace cartocrow::simplification {

	template <typename P, typename K> concept SegmentConvertable = requires(P p) {

		{
			p.getSegment()
		} -> std::convertible_to<Segment<K>>;
	};

	template<typename P, typename K> requires SegmentConvertable<P, K>
	struct SegmentQuadTreeTraits {
		using Element = P;
		using Kernel = K;

		static Rectangle<Kernel> get_bounding_box(Element& elt) {
			Segment<Kernel> seg = elt.getSegment();
			return utils::boxOf({ seg.start(), seg.end() });
		}

		static bool element_overlaps_rectangle(Element& elt, Rectangle<Kernel>& rect) {
			Segment<Kernel> seg = elt.getSegment();
			return utils::overlaps(rect, seg);
		}
	};

	template<typename P, typename K>
		requires SegmentConvertable<P, K>
	using SegmentQuadTree = cartocrow::datastructures::QuadTree<SegmentQuadTreeTraits<P, K>>;
}