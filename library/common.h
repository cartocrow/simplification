#pragma once

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
}