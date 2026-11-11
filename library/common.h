#pragma once

#include <cartocrow/core/core.h>
#include <cartocrow/data_structures/indexed_priority_queue.h>
#include <cartocrow/data_structures/point_quad_tree.h>
#include <cartocrow/data_structures/quad_tree.h>
#include <cartocrow/data_structures/straight_graph_2.h>

#include "utils.h"

namespace cartocrow::simplification {

	template<class K>
	concept KernelType = std::is_same<K, Inexact>::value || std::is_same<K, Exact>::value;

	template<class G>
	concept GraphType = requires {
		typename G::Graph_traits;
	};

	struct EmptyBase {
		virtual ~EmptyBase() {}
	};

	template<class Elt>
	struct GraphQueueTraits {

		using Element_handle = Elt;

		static void setIndex(Element_handle elt, int index) {
			elt->data().queue_index = index;
		}

		static int getIndex(Element_handle elt) {
			return elt->data().queue_index;
		}

		static int compare(Element_handle a, Element_handle b) {
			const auto ac = a->data().cost;
			const auto bc = b->data().cost;
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

	template<class Elt, class DataTrait>
	struct StatefulGraphQueueTraits {

		using Element_handle = Elt;

		DataTrait* m_trait;

		StatefulGraphQueueTraits() : m_trait(nullptr) {}

		StatefulGraphQueueTraits(DataTrait& trait) : m_trait(&trait) {}

		void setIndex(Element_handle elt, int index) {
			m_trait->data(elt).queue_index = index;
		}

		int getIndex(Element_handle elt) const {
			return m_trait->data(elt).queue_index;
		}

		int compare(Element_handle a, Element_handle b) const {
			const auto ac = m_trait->data(a).cost;
			const auto bc = m_trait->data(b).cost;
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

	template<class Elt>
	struct DataQueueTraits {
		
		using Element_handle = Elt;

		static void setIndex(Element_handle elt, int index) {
			elt->queue_index = index;
		}

		static int getIndex(Element_handle elt) {
			return elt->queue_index;
		}

		static int compare(Element_handle a, Element_handle b) {
			const auto ac = a->cost;
			const auto bc = b->cost;
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


	template<GraphType Graph>
	struct VertexQuadTreeTraits {

		using Element = Graph::Vertex_handle;
		using Kernel = Graph::Kernel;

		static const Point<Kernel>& get_point(Element elt) {
			return elt->point();
		}
	};

	template<GraphType Graph>
	using VertexQuadTree = cartocrow::PointQuadTree<VertexQuadTreeTraits<Graph>>;

	template<GraphType Graph>
	struct EdgeQuadTreeTraits {
		using Element = Graph::Edge_handle;
		using Kernel = Graph::Kernel;

		static Rectangle<Kernel> get_bounding_box(Element elt) {
			Segment<Kernel> seg = elt->curve();
			return utils::boxOf({ seg.start(), seg.end() });
		}

		static bool element_overlaps_rectangle(Element elt, const Rectangle<Kernel>& rect) {
			Segment<Kernel> seg = elt->curve();
			return utils::overlaps(rect, seg);
		}
	};

	template<GraphType Graph>
	using EdgeQuadTree = cartocrow::QuadTree<EdgeQuadTreeTraits<Graph>>;


	template<int detaillevel>
	struct Debug {
		bool enabled = false;

		template<typename T>
		Debug& operator<<(T&& v) {
			if constexpr (detaillevel >= 0) {
				if (enabled) {
					std::cout << std::forward<T>(v);
				}
			}
			return *this;
		}

		Debug& operator<<(std::ostream& (*m)(std::ostream&)) {
			if constexpr (detaillevel >= 0) {
				if (enabled) {
					std::cout << m;
				}
			}
			return *this;
		}

		Debug& operator[](int level) {
			if constexpr (detaillevel >= 0) {
				enabled = level <= detaillevel;
			}
			return *this;
		}
	};
}