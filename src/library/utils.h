#pragma once

namespace cartocrow::simplification::utils {

	template <typename Kernel>
	Rectangle<Kernel> boxOf(Point<Kernel>& a, Point<Kernel>& b, Point<Kernel>& c) {
		Number<Kernel> left = CGAL::min(a.x(), CGAL::min(b.x(), c.x()));
		Number<Kernel> right = CGAL::max(a.x(), CGAL::max(b.x(), c.x()));
		Number<Kernel> bottom = CGAL::min(a.y(), CGAL::min(b.y(), c.y()));
		Number<Kernel> top = CGAL::max(a.y(), CGAL::max(b.y(), c.y()));

		return Rectangle<Kernel>(left, bottom, right, top);
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