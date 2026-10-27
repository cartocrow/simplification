#pragma once

#include "path_vertex_removal.h"

namespace cartocrow::simplification {

	namespace detail {
		template<KernelType Kernel>
		struct AreaIncreasedData {
			Number<Kernel> left_area_increased;
		};

		template<pvr::DataTraits DT>
		class PreservingScoreTraits {
		public:
			using Graph = DT::Graph;
			using Vertex_data = DT::Vertex_data;
			using Path_data = DT::Path_data;
			using Kernel = DT::Graph::Kernel;

			PreservingScoreTraits() : m_weight(1) {
			}
			PreservingScoreTraits(Number<Kernel> weight) : m_weight(weight) {
			}

			void initialize(Graph& graph, DT& data) {
				// skip
			}

			Number<Kernel> compute_cost(Vertex_data& vd) {
				auto v = static_cast<typename Graph::Vertex_handle>(vd.vertex);
				return CGAL::abs(
					CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
			}

			Number<Kernel> compute_cost(Path_data& pd, Vertex_data& vd) {
				return vd.cost + (vd.left ? -m_weight : m_weight) * pd.left_area_increased;
			}

			void update(Path_data& pd, Vertex_data& vd) {
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
		using PreservingDataTraits_Generic = pvr::GenericDataTraits<G, EmptyBase, AreaIncreasedData<typename G::Kernel>>;

		template<KernelType Kernel, bool Historic>
		using PreservingDataTraits_Dedicated = pvr::GraphDataTraits<Kernel, Historic, EmptyBase, AreaIncreasedData<Kernel>>;
	} // namespace detail

	template <GraphType G>
	using PreservingVisvalingamWhyatt_Generic = pvr::PathVertexRemoval<
		detail::PreservingDataTraits_Generic<G>,
		detail::PreservingScoreTraits<detail::PreservingDataTraits_Generic<G>>
	>;

	template<KernelType Kernel, bool Historic>
	using PreservingVisvalingamWhyatt_Dedicated = pvr::PathVertexRemoval<
		detail::PreservingDataTraits_Dedicated<Kernel, Historic>,
		detail::PreservingScoreTraits<detail::PreservingDataTraits_Dedicated<Kernel, Historic>>
	>;

} // namespace cartocrow::simplification