#pragma once

#include <cartocrow/core/core.h>

#include "point_quad_tree.h"
#include "straight_graph.h"
#include "indexed_priority_queue.h"
#include "modifiable_graph.h"
#include "historic_graph.h"

namespace cartocrow::simplification {

	namespace detail {
		template <typename K> struct VRData;

		template <typename K> struct HVRData;

		template <typename K> struct HVREdge;

		template<typename K>
		using HVRGraph = StraightGraph<HVRData<K>, HVREdge<K>, K>;

		template <class MG, class VRT>
		concept VRSetup = requires(MG::Vertex * v) {
			requires ModifiableGraph<MG>;

			requires std::same_as<typename MG::Kernel, typename VRT::Kernel>;
		//requires std::is_base_of<typename VRData<typename MG::Kernel>, typename MG::Edge::Data>;
		{
			VRT::getCost(v)
		} -> std::same_as<Number<typename VRT::Kernel>>;
		};

		template<class Vertex, class Kernel>
		struct VRQueueTraits;
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
			IndexedPriorityQueue<Vertex, detail::VRQueueTraits<Vertex,Kernel>> queue;

			void update(Vertex* v);
			Rectangle<Kernel> boxOf(Point<Kernel>& a, Point<Kernel>& b, Point<Kernel>& c);

		public:
			VertexRemoval(MG& g, PointQuadTree<Vertex, Kernel>& qt) : graph(g), pqt(qt) {}
			~VertexRemoval() {}

			void initialize(bool initQuadTree);
			bool runToComplexity(int k);
			bool step();
	};

} // namespace cartocrow::simplification

#include "vertex_removal.hpp"