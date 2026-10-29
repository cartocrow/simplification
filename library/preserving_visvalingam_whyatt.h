#pragma once

#include "path_vertex_removal.h"

namespace cartocrow::simplification {

	namespace pvw_detail {
		template<KernelType Kernel>
		struct AreaIncreasedData {
			Number<Kernel> left_area_increased;
		};

		template<pvr::DataTraits DT>
		class ScoreTraits {
		public:
			using Graph = DT::Graph;
			using Vertex_data = DT::Vertex_data;
			using Path_data = DT::Path_data;
			using Kernel = DT::Graph::Kernel;

			ScoreTraits() : m_weight(1) {
			}
			ScoreTraits(Number<Kernel> weight) : m_weight(weight) {
			}

			static void initialize(Graph& graph, DT& data) {
				// skip
			}

			static Number<Kernel> compute_cost(Vertex_data& vd) {
				auto v = static_cast<typename Graph::Vertex_handle>(vd.vertex);
				return CGAL::abs(
					CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
			}

			Number<Kernel> compute_cost(Path_data& pd, Vertex_data& vd) {
				return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
			}

			static void update(Path_data& pd, Vertex_data& vd) {
				if (vd.left) {
					pd.left_area_increased -= vd.cost;
				}
				else {
					pd.left_area_increased += vd.cost;
				}
			}
		private:
			Number<Kernel> m_weight;
		};

		template <GraphType G>
		using DataTraits_Generic = pvr::DataTraits_Generic<G, EmptyBase, AreaIncreasedData<typename G::Kernel>>;

		template<KernelType Kernel, bool Historic>
		using DataTraits_Dedicated = pvr::DataTraits_Dedicated<Kernel, Historic, EmptyBase, AreaIncreasedData<Kernel>>;
	} // namespace pvw_detail

	template <GraphType G>
	using PreservingVisvalingamWhyatt_Generic = pvr::PathVertexRemoval<
		pvw_detail::DataTraits_Generic<G>,
		pvw_detail::ScoreTraits<pvw_detail::DataTraits_Generic<G>>
	>;

	template<KernelType Kernel, bool Historic>
	using PreservingVisvalingamWhyatt_Dedicated = pvr::PathVertexRemoval<
		pvw_detail::DataTraits_Dedicated<Kernel, Historic>,
		pvw_detail::ScoreTraits<pvw_detail::DataTraits_Dedicated<Kernel, Historic>>
	>;

} // namespace cartocrow::simplification