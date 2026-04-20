#pragma once

namespace cartocrow::simplification {

	template<class Elt, typename Kernel>
	struct GraphQueueTraits {

		using Element_handle = Elt*;

		static void setIndex(Element_handle elt, int index) {
			elt->data().qid = index;
		}

		static int getIndex(Element_handle elt) {
			return elt->data().qid;
		}

		static int compare(Element_handle a, Element_handle b) {
			Number<Kernel> ac = a->data().cost;
			Number<Kernel> bc = b->data().cost;
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