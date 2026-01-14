#pragma once

#include <cartocrow/core/core.h>

#include "point_quad_tree.h"
#include "segment_quad_tree.h"
#include "straight_graph.h"
#include "indexed_priority_queue.h"
#include "modifiable_graph.h"
#include "historic_graph.h"

#include "common.h"

namespace cartocrow::simplification {

	namespace detail {
		template <class MG, class ECT>
		concept ECSetup = requires(MG::Edge * e) {
			requires ModifiableGraph<MG>;

			requires std::same_as<typename MG::Kernel, typename ECT::Kernel>;

		{
			e->data().cost
		} -> std::same_as<Number<typename MG::Kernel>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.

		{
			e->data().blocked_by
		} -> std::same_as<std::vector<typename MG::Edge*>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.

		{
			e->data().blocking
		} -> std::same_as<std::vector<typename MG::Edge*>&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.

		{
			e->data().qid
		} -> std::same_as<int&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.


		{
			ECT::determineCollapse(e)
		};
		};

		template <typename K> struct ECData;
		template <typename K> struct HECData;

		template<typename K>
		using HECGraph = StraightGraph<std::monostate, HECData<K>, K>;
	}

	/// <summary>
	/// Graph type that can be used with the EdgeCollapse implementation. This variant is oblivious: changes made to the graph are not recoverable.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using EdgeCollapseGraph = StraightGraph<std::monostate, detail::ECData<K>, K>;

	/// <summary>
	/// Graph type that can be used with the EdgeCollapse implementation. This variant is historic: changes made to the graph can be undone and redone to retrieve intermediate steps.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using HistoricEdgeCollapseGraph = HistoricGraph<detail::HECGraph<K>>;


	template <class MG, class ECT> requires detail::ECSetup<MG, ECT> class EdgeCollapse {
	public:
		using Vertex = MG::Vertex;
		using Edge = MG::Edge;
		using Kernel = MG::Kernel;
	private:
		MG& graph;
		SegmentQuadTree<Edge, Kernel>& sqt;
		PointQuadTree<Vertex, Kernel>& pqt;
		IndexedPriorityQueue<GraphQueueTraits<Edge, Kernel>> queue;

		void update(Edge* e);
		bool blocks(Edge& edge, Edge* collapse);
		bool validateState();

		Edge* findNextStep();
		void performStep(Edge* e);
	public:
		EdgeCollapse(MG& g, SegmentQuadTree<Edge, Kernel>& sqt, PointQuadTree<Vertex, Kernel>& pqt);
		~EdgeCollapse();

		void initialize(bool initSQT, bool initPQT);
		bool run(std::optional<std::function<bool(int,Number<Kernel>)>> stop = std::nullopt);
		bool step();

	};


	template <typename G> struct KronenfeldEtAlTraits {
		using Kernel = G::Kernel;

		static void determineCollapse(typename G::Edge* e);
	};

	template <typename G> using KronenfeldEtAl = EdgeCollapse<G, KronenfeldEtAlTraits<G>>;

} // namespace cartocrow::simplification

#include "edge_collapse.hpp"