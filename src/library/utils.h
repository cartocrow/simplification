#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification::utils {

	template <typename K>
	Rectangle<K> boxOf(Point<K>& a, Point<K>& b, Point<K>& c) {
		Number<K> left = CGAL::min(a.x(), CGAL::min(b.x(), c.x()));
		Number<K> right = CGAL::max(a.x(), CGAL::max(b.x(), c.x()));
		Number<K> bottom = CGAL::min(a.y(), CGAL::min(b.y(), c.y()));
		Number<K> top = CGAL::max(a.y(), CGAL::max(b.y(), c.y()));

		return Rectangle<K>(left, bottom, right, top);
	}

	template<typename K>
	Rectangle<K> boxOf(Triangle<K>& T1, Triangle<K>& T2) {

		Number<K> left = CGAL::min(CGAL::min(T1[0].x(), CGAL::min(T1[1].x(), T1[2].x())),
			CGAL::min(T2[0].x(), CGAL::min(T2[1].x(), T2[2].x())));
		Number<K> right = CGAL::max(CGAL::max(T1[0].x(), CGAL::max(T1[1].x(), T1[2].x())),
			CGAL::max(T2[0].x(), CGAL::max(T2[1].x(), T2[2].x())));

		Number<K> bottom = CGAL::min(CGAL::min(T1[0].y(), CGAL::min(T1[1].y(), T1[2].y())),
			CGAL::min(T2[0].y(), CGAL::min(T2[1].y(), T2[2].y())));
		Number<K> top = CGAL::max(CGAL::max(T1[0].y(), CGAL::max(T1[1].y(), T1[2].y())),
			CGAL::max(T2[0].y(), CGAL::max(T2[1].y(), T2[2].y())));

		return Rectangle<K>(left, bottom, right, top);
	}

	template<typename K>
	bool encloses(Rectangle<K>& larger, Rectangle<K>& smaller) {
		return larger.xmin() <= smaller.xmin() && larger.ymin() <= smaller.ymin() &&
			larger.xmax() >= smaller.xmax() && larger.ymax() >= smaller.ymax();
	}

	template<typename K>
	bool disjoint(Rectangle<K>& a, Rectangle<K>& b) {
		return a.xmax() < b.xmin() || a.xmin() > b.xmax() || a.ymax() < b.ymin() ||
			a.ymin() > b.ymax();
	}

	template<typename K>
	bool contains(Rectangle<K>& a, Point<K>& pt) {
		return a.xmin() <= pt.x() && pt.x() <= a.xmax() && a.ymin() <= pt.y() && pt.y() <= a.ymax();
	}

	template<typename T>
	bool listRemove(T* elt, std::vector<T*>& vec) {

		auto pos = std::find(vec.begin(), vec.end(), elt);
		if (pos != vec.end()) {
			vec.erase(pos);
			return true;
		}
		else {
			return false;
		}
	}

	template<typename T>
	T* swapRemove(int index, std::vector<T*>& vec) {
		if (index == vec.size() - 1) {
			vec.pop_back();
			return nullptr;
		}
		else {
			vec[index] = vec[vec.size() - 1];
			vec.pop_back();
			return vec[index];
		}
	}

	template<typename T>
	void listReplace(T* oldelt, T* newelt, std::vector<T*>& vec) {
		for (int i = 0; i < vec.size(); i++) {
			if (vec[i] == oldelt) {
				vec[i] = newelt;
				return;
			}

		}
		assert(false);

	}
}