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
}