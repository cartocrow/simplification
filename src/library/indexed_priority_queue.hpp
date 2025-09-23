// -----------------------------------------------------------------------------
// IMPLEMENTATION OF TEMPLATE FUNCTIONS
// Do not include this file, but the .h file instead
// -----------------------------------------------------------------------------
 
namespace cartocrow::simplification {

	template <class T, class QT> requires QueueTraits<T, QT>
	void IndexedPriorityQueue<T, QT>::siftUp(int k, T* elt) {
		while (k > 0) {
			int parent = (k - 1) >> 1;
			T* e = queue[parent];
			if (QT::compare(elt, e) >= 0) {
				break;
			}
			queue[k] = e;
			QT::setIndex(e, k);
			k = parent;
		}
		queue[k] = elt;
		QT::setIndex(elt, k);
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	void IndexedPriorityQueue<T, QT>::siftDown(int k, T* elt) {
		int half = queue.size() >> 1;
		while (k < half) {
			int child = (k << 1) + 1;
			T* c = queue[child];
			int right = child + 1;
			if (right < queue.size() && QT::compare(c, queue[right]) > 0) {
				c = queue[child = right];
			}
			if (QT::compare(elt, c) <= 0) {
				break;
			}
			queue[k] = c;
			QT::setIndex(c, k);
			k = child;
		}
		queue[k] = elt;
		QT::setIndex(elt, k);
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	bool IndexedPriorityQueue<T, QT>::empty() {
		return queue.empty();
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	void IndexedPriorityQueue<T, QT>::push(T* elt) {
		queue.push_back(elt);
		siftUp(queue.size() - 1, elt);
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	T* IndexedPriorityQueue<T, QT>::pop() {
		if (queue.empty()) {
			return nullptr;
		}
		T* result = queue[0];
		QT::setIndex(result, -1);

		T* last = queue[queue.size() - 1];
		queue.pop_back();
		if (!queue.empty()) {
			siftDown(0, last);
		}

		return result;
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	bool IndexedPriorityQueue<T, QT>::remove(T* elt) {
		int id = QT::getIndex(elt);
		if (id < 0 || id >= queue.size() || queue[id] != elt) {
			return false;
		}
		else {
			QT::setIndex(elt, -1);
			if (id == queue.size() - 1) {
				queue.pop_back();
			}
			else {
				T* moved = queue[queue.size() - 1];
				queue.pop_back();
				siftDown(id, moved);
				if (queue[id] == moved) {
					siftDown(id, moved);
				}
			}

			return true;
		}
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	bool IndexedPriorityQueue<T, QT>::contains(T* elt) {
		int id = QT::getIndex(elt);
		if (id < 0 || id >= queue.size()) {
			return false;
		}
		else {
			return queue[id] == elt;
		}
	}

	template <class T, class QT> requires QueueTraits<T, QT>
	void IndexedPriorityQueue<T, QT>::update(T* elt) {
		siftUp(QT::getIndex(elt), elt);
		siftDown(QT::getIndex(elt), elt);
	}

} // namespace cartocrow::simplification