#pragma once

namespace cartocrow::simplification {

	namespace detail {

		template<ModifiableGraph MG> struct BaseMove;
		template<ModifiableGraph MG> struct SingleMove;
		template<ModifiableGraph MG> struct ComboMove;

		template <class MG, class EMT>
		concept EMSetup = requires(MG::Edge * e, BaseMove<MG>&m, SingleMove<MG>&sm, ComboMove<MG>&cm) {
			requires ModifiableGraph<MG>;

			requires std::same_as<typename MG::Kernel, typename EMT::Kernel>;

		{
			e->data().left
		} -> std::same_as<SingleMove<MG>&>;

		{
			e->data().right
		} -> std::same_as<SingleMove<MG>&>;

		{
			e->data().combo
		} -> std::same_as<ComboMove<MG>&>;

		{
			e->data().blocking
		} -> std::same_as<std::vector<BaseMove<MG>*>&>;

		{
			EMT::determineSingleCost(sm)
		};

		{
			EMT::determineComboCost(cm)
		};

		};

		template <typename K> struct EMData;
		template <typename K> struct HEMData;

		template<typename K>
		using HEMGraph = StraightGraph<VoidData, HEMData<K>, K>;

		template<ModifiableGraph MG>
		struct MoveQueueTraits;
	}

	/// <summary>
	/// Graph type that can be used with the EdgeCollapse implementation. This variant is oblivious: changes made to the graph are not recoverable.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using EdgeMoveGraph = StraightGraph<VoidData, detail::EMData<K>, K>;

	/// <summary>
	/// Graph type that can be used with the EdgeCollapse implementation. This variant is historic: changes made to the graph can be undone and redone to retrieve intermediate steps.
	/// </summary>
	/// <typeparam name="K">Desired CGAL kernel</typeparam>
	template<typename K>
	using HistoricEdgeMoveGraph = HistoricGraph<detail::HEMGraph<K>>;

	template <class MG, class EMT> requires detail::EMSetup<MG, EMT> class EdgeMove {
	public:
		using Vertex = MG::Vertex;
		using Edge = MG::Edge;
		using Kernel = MG::Kernel;
	private:
		using Move = detail::BaseMove<MG>;
		using Single = detail::SingleMove<MG>;
		using Combo = detail::ComboMove<MG>;

		MG& graph;
		SegmentQuadTree<Edge, Kernel>& sqt;
		PointQuadTree<Vertex, Kernel>& pqt;
		IndexedPriorityQueue<detail::MoveQueueTraits<MG>> queue;

		void update(Edge* e);
		bool blocks(Edge& edge, Move& move);

	public:
		EdgeMove(MG& g, SegmentQuadTree<Edge, Kernel>& sqt, PointQuadTree<Vertex, Kernel>& pqt);
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