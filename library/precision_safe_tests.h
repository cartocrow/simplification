#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::safe_test {

	constexpr const Number<Inexact> M_EPSILON_SQRD = M_EPSILON * M_EPSILON;

	inline bool close(const Number<Exact> a, const Number<Exact> b) {
		return a == b;
	}
	inline bool close(const Number<Inexact> a, const Number<Inexact> b) {
		return std::abs(a - b) < M_EPSILON;
	}

	inline bool same_point(const Point<Exact> a, const Point<Exact> b) {
		return a == b;
	}
	inline bool same_point(const Point<Inexact> a, const Point<Inexact> b) {
		return std::abs(a.x() - b.x()) < M_EPSILON
			&& std::abs(a.y() - b.y()) < M_EPSILON;
	}

	inline bool aligned_vectors(const Vector<Exact> a, const Vector<Exact> b) {
		return a.direction() == b.direction();
	}
	inline bool aligned_vectors(const Vector<Inexact> a, const Vector<Inexact> b) {
		return close(CGAL::determinant(a, b), M_EPSILON);
	}

	inline bool collinear(const Point<Exact> a, const Point<Exact> b, const Point<Exact> c) {
		return CGAL::collinear(a, b, c);
	}
	inline bool collinear(const Point<Inexact> a, const Point<Inexact> b, const Point<Inexact> c) {
		return CGAL::squared_distance(Line<Inexact>(a, c), b) < M_EPSILON_SQRD;
	}

	inline bool point_on_line(const Point<Exact> p, const Line<Exact> l) {
		return l.has_on_boundary(p);
	}
	inline bool point_on_line(const Point<Inexact> p, const Line<Inexact> l) {
		return CGAL::squared_distance(l, p) < M_EPSILON_SQRD;
	}


}

