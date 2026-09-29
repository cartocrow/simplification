#include "../library/vertex_removal.h"
//#include "../library/edge_collapse.h"
//#include "../library/edge_moves.h"
#include "straight_simplification_algorithm.h"

namespace CGAL {
/// The vertex-removal simplification algorithm for straight-edge geometries by Visvalingam and Whyatt.
/// The algorithm iteratively removes the degree-2 vertex v such that the triangle defined by v and its two neighbors has smallest area.
/// @tparam K 2D linear kernel.
/// @tparam H whether to store the simplification operations for efficient backtracking.
template<class K, bool H>
class VW : public internal::Straight_simplification_algorithm<K, VW<K, H>, cartocrow::simplification::VisvalingamWhyatt<cartocrow::simplification::VertexRemovalGraph<K, H>>> {
	// all methods are inherited from the base class
};

/// The edge-collapse simplification algorithm for straight-edge geometries by Kronenfeld, Stanislawski, Buttenfield, and Brockmeyer.
/// The algorithm iteratively replaces three consecutive edge by two, effectively collapsing an edge to a point.
/// The collapse is done such that area is preserved exactly (up to floating point errors).
/// The algorithm minimizes the area of symmetric difference of each step with the previous step.
/// @tparam K 2D linear kernel.
/// @tparam H whether to store simplification operations for efficient backtracking.
//template<class K, bool H>
//class KSBB  : ... {
//	// all methods are inherited from the base class
//}

/// The schematization algorithm for straight-edge geometries by Buchin, Meulemans, Renssen, Speckmann.
/// @tparam K 2D linear kernel.
/// @tparam H whether to store simplification operations for efficient backtracking.
//template<class K, bool H>
//class BMRS : ... {
//    /// Convert the subdivision into one where all edges are aligned with one of the given directions.
//	/// @tparam InputIterator iterator over CGAL::Direction_2<K>.
//	template <class InputIterator>
//	void orient(InputIterator begin, InputIterator end);
//}
}