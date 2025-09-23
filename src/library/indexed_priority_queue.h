#pragma once

namespace cartocrow::simplification {

	template <class T, class QT> concept QueueTraits = requires(T * elt, T * elt2, int i) {
		{ QT::setIndex(elt, i) };

		{
			QT::getIndex(elt)
		} -> std::same_as<int>;

		{
			QT::compare(elt, elt2)
		} -> std::same_as<int>; // negative if elt < elt2, positive if elt > elt2, zero if elt = elt2. Smallest value == highest priority (top of queue)
	};

	template <class T, class QT> requires QueueTraits<T, QT> class IndexedPriorityQueue {
	private:
		std::vector<T*> queue;

		void siftUp(int k, T* elt);
		void siftDown(int k, T* elt);

	public:
		bool empty();

		void push(T* elt);
		T* pop();

		bool remove(T* elt);
		bool contains(T* elt);
		void update(T* elt);
	};
} // namespace cartocrow::simplification

#include "indexed_priority_queue.hpp"