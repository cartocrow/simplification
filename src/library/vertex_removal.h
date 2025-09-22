#pragma once

#include <cartocrow/core/core.h>

#include "point_quad_tree.h"
#include "straight_graph.h"
#include "indexed_priority_queue.h"
#include "modifiable_graph.h"
#include "historic_graph.h"

namespace cartocrow::simplification {

	//-------------- NO HISTORY --------------------------------------------------

	namespace detail {
		template <typename K> struct VRData;
	}

	template<typename K>
	using VertexRemovalGraph = StraightGraph<detail::VRData<K>, VoidData, K>;


	//-------------- WITH HISTORY ------------------------------------------------


	namespace detail {
		template <typename K> struct HVRData;

		template <typename K> struct HVREdge;

		template<typename K>
		using HVRGraph = StraightGraph<HVRData<K>, HVREdge<K>, K>;
	}

	template<typename K>
	using HistoricVertexRemovalGraph = HistoricGraph<detail::HVRGraph<K>>;

	//-------------- VERTEX REMOVAL ----------------------------------------------

	namespace detail {
		template <class MG, class VRT>
		concept VRSetup = requires(MG::Vertex * v) {
			requires ModifiableGraph<MG>;

			requires std::same_as<typename MG::Kernel, typename VRT::Kernel>;
		//requires std::is_base_of<typename VRData<typename MG::Kernel>, typename MG::Edge::Data>;
		{
			VRT::getCost(v)
		} -> std::same_as<Number<typename VRT::Kernel>>;
		};
	}

	template <class MG, class VRT>
		requires detail::VRSetup<MG, VRT> class VertexRemoval {

		public:
			using Vertex = MG::Vertex;
			using Kernel = MG::Kernel;

		private:
			struct VRQueueTraits {

				static void setIndex(Vertex* elt, int index) {
					elt->data().qid = index;
				}

				static int getIndex(Vertex* elt) {
					return elt->data().qid;
				}

				static int compare(Vertex* a, Vertex* b) {
					Number<Kernel> ac = a->data().cost;
					Number<Kernel> bc = b->data().cost;
					if (ac < bc) {
						return -1;
					}
					else if (ac > bc) {
						return 1;
					}
					else {
						return 0;
					}
				}
			};

			MG& graph;
			PointQuadTree<Vertex, Kernel>& pqt;
			IndexedPriorityQueue<Vertex, VRQueueTraits> queue;

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