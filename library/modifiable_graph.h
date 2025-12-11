#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

template <class MG>
concept ModifiableGraph =
    requires(MG mg, typename MG::Vertex* v, typename MG::Edge* e, Point<typename MG::Kernel> p) {

	typename MG::Vertex;
	typename MG::Edge;
	typename MG::Kernel;

	/// Retrieves the vertices
	{
		mg.getVertices()
	} -> std::same_as<std::vector<typename MG::Vertex*>&>;

	/// Retrieves the edges
	{
		mg.getEdges()
	} -> std::same_as<std::vector<typename MG::Edge*>&>;

	/// Retrieves the edges
	{
		mg.getEdgeCount()
	} -> std::same_as<int>;

	/// Assuming the given vertex is of degree 2, the vertex is erased, introducing a new edge between its neighbors.
	/// If the edges were consistently oriented, then the new edge should match this orientation. The new edge is returned.
	{
		mg.mergeVertex(v)
	} -> std::same_as<typename MG::Edge*>;

	/// Splits an \f$e = (u,w)\f$, creating a new degree-2 vertex \f$v\f$ at the given
	/// location. The new edges are \f$(u,v)\f$ and \f$(v,w)\f$, i.e., oriented according to the original edge. The new vertex is returned.
	{
		mg.splitEdge(e, p)
	} -> std::same_as<typename MG::Vertex*>;

	/// Moves the vertex \f$v\f$ to the indicated location.
	{mg.shiftVertex(v, p)};
};

template <class MG>
concept ModifiableGraphWithHistory = requires(MG mg, Number<typename MG::Kernel> c) {

	requires ModifiableGraph<MG>;

	/// Sets the graph to its latest computed result
	{mg.goToPresent()};

	/// Starts a batch of operations, that together incur a cost c
	{mg.startBatch(c)};
	/// Ends a batch of operations
	{mg.endBatch()};
};

} // namespace cartocrow::simplification