#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/straight_graph_2.h>
#include <cartocrow/data_structures/graph_traits_2.h>

#include "vertex_quad_tree.h"
#include "common.h"

namespace cartocrow::simplification {

	namespace detail {

		enum DataStorageType {
			DEDICATED, GENERIC
		};

		template<class PVRT>
		struct PVRData;

		template<class PVRT>
		struct PVRPathData;

		template<class PVRT, class K, bool Historic>
		struct PVRTraits_DedicatedBase;

		template<class PVRT, class G>
		struct PVRTraits_GenericBase;

		template<class Self, DataStorageType type, class GraphOrKernel, bool Historic>
		using PVRTraits_Base = std::conditional<type == DEDICATED,
			detail::PVRTraits_DedicatedBase<Self, GraphOrKernel, Historic>,
			detail::PVRTraits_GenericBase<Self, GraphOrKernel>
		>::type;

		template<class PVRT>
		class PVRPathQueueTraits;

		template<class PVRT>
		concept PVRTraits =
			PVRT::Graph::Graph_traits::oriented &&
			PVRT::Graph::Graph_traits::decomposed &&
			requires(PVRT traits, typename PVRT::Graph graph, typename PVRT::Graph::Vertex_handle v, typename PVRT::Graph::Path_handle p) {

			typename PVRT::Graph;

			PVRT::use_graph_data;
				requires std::is_same_v<bool const, decltype(PVRT::use_graph_data)>;

			{ traits.initialize(graph) };
			{ traits.data(v) } -> std::same_as<PVRData<PVRT>&>;
			{ traits.data(p) } -> std::same_as<PVRPathData<PVRT>&>;
			{ traits.compute_cost(v) } -> std::same_as<Number<typename PVRT::Graph::Kernel>>;
			{ traits.compute_cost(p, v) } -> std::same_as<Number<typename PVRT::Graph::Kernel>>;
		};
	}

	template <detail::PVRTraits PVRT> class PreservingVertexRemoval {

	public:
		using Traits = PVRT;
		using Graph = PVRT::Graph;
		using Vertex_handle = Graph::Vertex_handle;
		using Path_handle = Graph::Path_handle;
		using Kernel = Graph::Kernel;
		using VertexTree = VertexQuadTree<Graph>;

	private:
		using Queue = cartocrow::data_structures::IndexedPriorityQueue<detail::PVRPathQueueTraits<PVRT>>;

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
		
	template<detail::DataStorageType type, class GraphOrKernel, bool Historic = false>
	class PreservingVisvalingamWhyattTraits : public detail::PVRTraits_Base<PreservingVisvalingamWhyattTraits<type, GraphOrKernel, Historic>, type, GraphOrKernel, Historic> {

	public:
		using Self = PreservingVisvalingamWhyattTraits<type, GraphOrKernel, Historic>;
		using Base = detail::PVRTraits_Base<Self, type, GraphOrKernel, Historic>;
		using Graph = Base::Graph;
		using Kernel = Base::Kernel;

		PreservingVisvalingamWhyattTraits() : m_weight(1) {
		}
		PreservingVisvalingamWhyattTraits(Number<Kernel> weight) : m_weight(weight) {
		}

		Number<Kernel> compute_cost(typename Graph::Vertex_handle v) {
			return CGAL::abs(
				CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
		}

		Number<Kernel> compute_cost(typename Graph::Path_handle p, typename Graph::Vertex_handle v) {
			auto& vd = this->data(v);
			auto& pd = this->data(p);
			return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
		};
	private:
		Number<Kernel> m_weight;
	};

	template <class G>
	using PreservingVisvalingamWhyatt_Generic = PreservingVertexRemoval<PreservingVisvalingamWhyattTraits<detail::GENERIC, G>>;

	
	template<class Kernel, bool Historic>
	using PreservingVisvalingamWhyatt_Dedicated = PreservingVertexRemoval<PreservingVisvalingamWhyattTraits<detail::DEDICATED, Kernel, Historic>>;

} // namespace cartocrow::simplification

#include "preserving_vertex_removal.hpp"