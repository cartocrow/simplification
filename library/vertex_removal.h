#pragma once

#include <cartocrow/core/core.h>

#include "point_quad_tree.h"
#include "straight_graph.h"
#include "indexed_priority_queue.h"
#include "modifiable_graph.h"
#include "historic_graph.h"

#include "common.h"

namespace cartocrow::simplification {

	namespace detail {

		template <class MG, class VRT>
		concept VRSetup = requires(MG::Vertex * v) {
			requires ModifiableGraph<MG>;

			requires std::same_as<typename MG::Kernel, typename VRT::Kernel>;

		    {
		    	v->data().cost
		    } -> std::same_as<Number<typename MG::Kernel>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
		    
		    {
		    	v->data().blocked_by
		    } -> std::same_as<std::vector<typename MG::Vertex*>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
		    
		    {
		    	v->data().blocking
		    } -> std::same_as<std::vector<typename MG::Vertex*>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
		    
		    {
		    	v->data().qid
		    } -> std::same_as<int&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
		    
		    {
		    	VRT::getCost(v)
		    } -> std::same_as<Number<typename VRT::Kernel>>;
		};

		template <typename K> struct VRData;

		template <typename K> struct HVRData;

		template <typename K> struct HVREdge;

		template<typename K>
		using HVRGraph = StraightGraph<HVRData<K>, HVREdge<K>, K>;
	}

	/// <summary>
	/// Graph type that can be used with the VertexRemoval implementation. This variant is oblivious: changes made to the graph are not recoverable.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using VertexRemovalGraph = StraightGraph<detail::VRData<K>, VoidData, K>;

	/// <summary>
	/// Graph type that can be used with the VertexRemoval implementation. This variant is historic: changes made to the graph can be undone and redone to retrieve intermediate steps.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using HistoricVertexRemovalGraph = HistoricGraph<detail::HVRGraph<K>>;

	/// <summary>
	/// The Vertex Removal algorithm. It is topologically safe, ensuring that vertices are only erased if they have degree 2 and the triangle spanned with its neighbors is empty. Can be configured with custom cost function, via the VertexRemovalTraits.
	/// </summary>
	/// <typeparam name="MG">Modifiable Graph type to be used; typically, will be one of VertexRemovalGraph or HistoricVertexRemovalGraph</typeparam>
	/// <typeparam name="VRT">VertexRemovalTraits, specifying the desired cost function</typeparam>
	template <class MG, class VRT>
		requires detail::VRSetup<MG, VRT> class VertexRemoval {

		public:
			using Vertex = MG::Vertex;
			using Kernel = MG::Kernel;

		private:
			MG& graph;
			PointQuadTree<Vertex, Kernel>& pqt;
			IndexedPriorityQueue<GraphQueueTraits<Vertex, Kernel>> queue;

			void update(Vertex* v);

			Vertex* findNextStep();
			void performStep(Vertex* v);
		public:
			VertexRemoval(MG& g, PointQuadTree<Vertex, Kernel>& qt) : graph(g), pqt(qt) {}
			~VertexRemoval() {}

			void initialize(bool initQuadTree);
			bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt);
			bool step();
	};


	/// <summary>
	/// Traits for running VisvalingamWhyatt vertex-removal algorithms. The cost is equal to the area of the spanned triangle.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <typename G> struct VisvalingamWhyattTraits {
		using Kernel = G::Kernel;

		static Number<Kernel> getCost(typename G::Vertex* v);
	};

	/// <summary>
	/// Shorthand for the VisvalingamWhyatt vertex-removal algorithm.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <typename G>
	using VisvalingamWhyatt = VertexRemoval<G, VisvalingamWhyattTraits<G>>;

} // namespace cartocrow::simplification

#include "vertex_removal.hpp"