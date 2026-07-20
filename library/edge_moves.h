#pragma once

#include <cartocrow/data_structures/indexed_priority_queue>
#include <cartocrow/data_structures/graph_2>
#include <cartocrow/data_structures/graph_2_traits>

#include "vertex_quad_tree.h"
#include "edge_quad_tree.h"

namespace cartocrow::simplification {

	namespace detail {

		template<bool H>
		using EMGraphTraits = CustomGraphTraits<true, true, false, std::monostate>;

		template<class G> struct BaseMove;
		template<class G> struct SingleMove;
		template<class G> struct ComboMove;

		template <class G> struct EMData;

		template <class EMT>
		concept EMTraits = requires(typename EMT::Graph::Edge_handle e, SingleMove<typename EMT::Graph>&sm, ComboMove<typename EMT::Graph>& cm) {
	
		{
			EMT::data(e)
		} -> std::same_as<EMData<EMT::Graph>>;

		{
			EMT::determineSingleCost(sm)
		};

		{
			EMT::determineComboCost(cm)
		};

		};

		template<class G>
		struct MoveQueueTraits;
	}

	template<typename K, bool H>
	using EdgeMoveGraph = straight_graph_2<std::monostate, detail::EMData<K>, K, EMGraphTraits<H>>;

	template <EMTraits EMT> class EdgeMove {
	public:
		using Graph = EMT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Edge_handle = Graph::Edge_handle;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;
		using EdgeTree = EdgeQuadTree<Graph>;

	private:
		using Move = detail::BaseMove<Graph>;
		using Single = detail::SingleMove<Graph>;
		using Combo = detail::ComboMove<Graph>;
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<detail::MoveQueueTraits<Graph>>;

		Graph& graph;
		EdgeTree& sqt;
		VertexTree& pqt;
		Queue queue;

		void update(Edge_handle e);
		bool blocks(Edge_handle edge, Move& move);

	public:
		EdgeMove(Graph& g, EdgeQuadTree& sqt, VertexQuadTree& pqt);
		~EdgeMove();

		void initialize(bool initSQT, bool initPQT);
		bool runToComplexity(int k);
		bool step();
	};


	template <typename G> struct BuchinEtAlTraits {
		using Kernel = G::Kernel;

		static void determineSingleCost(detail::SingleMove<G>& sm);

		static void determineComboCost(detail::ComboMove<G>& cm);
	};

	template <typename G> using BuchinEtAl = EdgeMove<G, BuchinEtAlTraits<G>>;

} // namespace cartocrow::simplification

#include "edge_moves.hpp"