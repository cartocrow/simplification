#include "../library/edge_collapse.h"
#include "straight_simplification_algorithm.h"

namespace CGAL {
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
}