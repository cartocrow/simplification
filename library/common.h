#pragma once

namespace cartocrow::simplification {

	template<class Elt, typename Kernel>
	struct GraphQueueTraits {

		using Element = Elt;

		static void setIndex(Elt* elt, int index) {
			elt->data().qid = index;
		}

		static int getIndex(Elt* elt) {
			return elt->data().qid;
		}

		static int compare(Elt* a, Elt* b) {
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