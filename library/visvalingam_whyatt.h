#pragma once

#include "vertex_removal.h"

namespace cartocrow::simplification {

	namespace vw_detail {

		template<vr::DataTraits DT>
		class ScoreTraits {
		public:
			using Graph = DT::Graph;
			using Vertex_data = DT::Vertex_data;
			using Kernel = DT::Graph::Kernel;

			static void initialize(Graph& graph, DT& data) {
				// skip
			}

			static Number<Kernel> compute_cost(Vertex_data& vd) {
				auto v = static_cast<typename Graph::Vertex_handle>(vd.vertex);
				return CGAL::abs(
					CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
			}


			static void update(Vertex_data& vd) {
				// skip
			}
		};

		template <GraphType G>
		using DataTraits_Generic = vr::DataTraits_Generic<G, EmptyBase>;

		template<KernelType Kernel, bool Historic>
		using DataTraits_Dedicated = vr::DataTraits_Dedicated<Kernel, Historic, EmptyBase>;
	} // namespace vw_detail

	template <GraphType G>
	using VisvalingamWhyatt_Generic = vr::VertexRemoval<
		vw_detail::DataTraits_Generic<G>,
		vw_detail::ScoreTraits<vw_detail::DataTraits_Generic<G>>
	>;

	template<KernelType Kernel, bool Historic>
	using VisvalingamWhyatt_Dedicated = vr::VertexRemoval<
		vw_detail::DataTraits_Dedicated<Kernel, Historic>,
		vw_detail::ScoreTraits<vw_detail::DataTraits_Dedicated<Kernel, Historic>>
	>;


	namespace vw_detail2 {

		template<vr2::DataTraits DT>
		class ScoreTraits {
		public:
			using Graph = DT::Graph;
			using Vertex_handle = Graph::Vertex_handle;
			using Vertex_data = DT::Vertex_data;
			using Kernel = DT::Graph::Kernel;

			static void initialize(Graph& graph, DT& data) {
				// skip
			}

			static Number<Kernel> compute_cost(Vertex_handle v, Vertex_data& vdata) {
				return CGAL::abs(
					CGAL::area(v->point(), v->prev()->point(), v->next()->point()));
			}


			static void update(Vertex_handle v, Vertex_data& vdata) {
				// skip
			}
		};

		template <GraphType G>
		using DataTraits_Generic = vr2::DataTraits_Generic<G, EmptyBase>;

		template<KernelType Kernel, bool Historic>
		using DataTraits_Dedicated = vr2::DataTraits_Dedicated<Kernel, Historic, EmptyBase>;
	} // namespace vw_detail

	template <GraphType G>
	using VisvalingamWhyatt_Generic2 = vr2::VertexRemoval<
		vw_detail2::DataTraits_Generic<G>,
		vw_detail2::ScoreTraits<vw_detail2::DataTraits_Generic<G>>
	>;

	template<KernelType Kernel, bool Historic>
	using VisvalingamWhyatt_Dedicated2 = vr2::VertexRemoval<
		vw_detail2::DataTraits_Dedicated<Kernel, Historic>,
		vw_detail2::ScoreTraits<vw_detail2::DataTraits_Dedicated<Kernel, Historic>>
	>;

} // namespace cartocrow::simplification