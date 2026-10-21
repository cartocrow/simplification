#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>

#include "vertex_quad_tree.h"
#include "common.h"

namespace cartocrow::simplification {

	namespace detail {

		template<class G>
		struct PVRData;

		template<class G>
		struct PVRPathData;

		template<class G> 
		struct PVRTraitsDataBase;

		template<class G>
		struct PVRTraitsGenericBase;

		template<class G>
		class PVRPathQueueTraits;

		template<class PVRT>
		concept PVRTraits =
			PVRT::Graph::Graph_traits::oriented &&
			PVRT::Graph::Graph_traits::decomposed &&
			requires(PVRT traits, typename PVRT::Graph graph, typename PVRT::Graph::Vertex_handle v, typename PVRT::Graph::Path_handle p) {

			typename PVRT::Graph;

			{ traits.initialize(graph) };
			{ traits.data(v) } -> std::same_as<PVRData<typename PVRT::Graph>&>;
			{ traits.compute_cost(v) } -> std::same_as<Number<typename PVRT::Graph::Kernel>>;
			{ traits.data(p) } -> std::same_as<PVRPathData<typename PVRT::Graph>&>;
			{ traits.compute_cost(p, v) } -> std::same_as<Number<typename PVRT::Graph::Kernel>>;
		};		

		template<class G>
		concept PVRGraph = std::same_as<typename G::Vertex_data, PVRData<G>>
			&& std::same_as<typename G::Graph_traits::PathData, PVRPathData<G>>;
	}

	template<typename K, bool H>
	struct PreservingVertexRemovalGraph : public Straight_graph_2<detail::PVRData<PreservingVertexRemovalGraph<K, H>>, std::monostate, K, DecomposedGraph<H, detail::PVRPathData<PreservingVertexRemovalGraph<K,H>>>> {};

	/// <summary>
	/// The Preserving Vertex Removal algorithm. It is topologically safe, ensuring that vertices are only erased if they have degree 2 and the triangle spanned with its neighbors is empty. Can be configured with custom cost function, via the VertexRemovalTraits.
	/// It does not give exact area preservation, but steers to operations that keep change in area small.
	/// </summary>
	/// <typeparam name="VRT">VertexRemovalTraits, specifying the desired cost function</typeparam>
	template <detail::PVRTraits PVRT> class PreservingVertexRemoval {

	public:
		using Traits = PVRT;
		using Graph = PVRT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Path_handle = Graph::Path_handle;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;

	private:
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<detail::PVRPathQueueTraits<Graph>>;

		Graph& graph;
		VertexTree& pqt;
		Queue queue;
		PVRT traits;

		void update(Vertex_handle v);
		void update(Path_handle p);

		Vertex_handle findNextStep();
		void performStep(Vertex_handle v);
	public:
		PreservingVertexRemoval(Graph& g, VertexTree& qt) requires std::is_default_constructible<PVRT>::value;
		PreservingVertexRemoval(Graph& g, VertexTree& qt, PVRT traits);

		void initialize(bool initQuadTree);
		bool run(std::optional<std::function<bool(int, Number<Kernel>)>> stop = std::nullopt);
		bool step();
	};


	/// <summary>
	/// Traits for running VisvalingamWhyatt vertex-removal algorithms. The cost is equal to the area of the spanned triangle.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <class G>
	class PreservingVisvalingamWhyattTraits : public std::conditional<detail::PVRGraph<G>, detail::PVRTraitsDataBase<G>, detail::PVRTraitsGenericBase<G>>::type {

	public:
		using Kernel = G::Kernel;

		PreservingVisvalingamWhyattTraits() : m_weight(1) {
		}
		PreservingVisvalingamWhyattTraits(Number<Kernel> weight) : m_weight(weight) {
		}

		Number<Kernel> compute_cost(typename G::Vertex_handle v) {
			return CGAL::abs(
				CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
		}

		Number<Kernel> compute_cost(typename G::Path_handle p, typename G::Vertex_handle v) {
			auto& vd = this->data(v);
			auto& pd = this->data(p);
			return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
		}
	private:
		const Number<Kernel> m_weight;
	};

	/// <summary>
	/// Shorthand for the VisvalingamWhyatt vertex-removal algorithm.
	/// </summary>
	/// <typeparam name="G">The graph type for the algorithm</typeparam>
	template <class G>
	using PreservingVisvalingamWhyatt = PreservingVertexRemoval<PreservingVisvalingamWhyattTraits<G>>;

} // namespace cartocrow::simplification

#include "preserving_vertex_removal.hpp"