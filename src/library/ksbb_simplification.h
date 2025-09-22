#pragma once

#include "edge_collapse.h"

namespace cartocrow::simplification {

template <typename K> struct KronenfeldEtAlTraits {
	static void determineCollapse(ECEdge<K>* e) {
		ECData<K>& edata = e->data();

		Point<K> a = e->sourceWalkNeighbor()->getPoint();
		Point<K> b = e->getSource()->getPoint();
		Point<K> c = e->getTarget()->getPoint();
		Point<K> d = e->targetWalkNeighbor()->getPoint();

		bool abc = CGAL::collinear(a, b, c);
		bool bcd = CGAL::collinear(b, c, d);
		if (abc && bcd) {
			edata.erase_both = true;
			edata.creates_difference = false;
			edata.cost = 0;
			return;
		} else if (abc) {
			edata.erase_both = false;
			edata.creates_difference = false;
			edata.cost = 0;
			edata.point = c;
			return;
		} else if (bcd) {
			edata.erase_both = false;
			edata.creates_difference = false;
			edata.cost = 0;
			edata.point = b;
			return;
		}

		// else, no consecutive collinear edges
		edata.creates_difference = true;

		Polygon<K> P;
		P.push_back(a);
		P.push_back(b);
		P.push_back(c);
		P.push_back(d);

		Line<K> ad(a, d);
		Line<K> ab(a, b);
		Line<K> bc(b, c);
		Line<K> cd(c, d);

		// area = base * height / 2
		// height = 2*area / base
		// so, we're going to rotate the vector d-a, such that we get a normal of length |d-a| = base.
		// To get a vector of length height, we then multiply this vector with height_times_base / base^2.
		// This normalized the vector and makes it length height! (without squareroots...)
		Number<K> height_times_base = 2 * P.area();

		Vector<K> perpv = (d - a).perpendicular(CGAL::CLOCKWISE);

		CGAL::Aff_transformation_2<K> s(CGAL::SCALING, height_times_base / perpv.squared_length());
		perpv = perpv.transform(s);

		CGAL::Aff_transformation_2<K> t(CGAL::TRANSLATION, perpv);
		Line<K> arealine = ad.transform(t);

		if (ad.has_on_boundary(arealine.point())) {

			// these should be caught already by the collinearity checks earlier
			assert(!ad.has_on_boundary(b));
			assert(!ad.has_on_boundary(c));

			edata.erase_both = true;

			// implies that neither b nor c is on ad
			auto intersection = CGAL::intersection(bc, ad);
			Point<K> pt = std::get<Point<K>>(*intersection);

			edata.T1 = Triangle<K>(a, b, pt);
			edata.T2 = Triangle<K>(c, d, pt);

		} else {
			edata.erase_both = false;

			bool ab_determines_shape;
			// determine type
			if (ad.has_on_positive_side(b) == ad.has_on_positive_side(c)) {
				// same side of ab, so further point determines
				ab_determines_shape = CGAL::squared_distance(b, ad) > CGAL::squared_distance(c, ad);
			} else {
				// opposite sides of ad, so the one that is on same side as area line
				ab_determines_shape =
				    ad.has_on_positive_side(b) == ad.has_on_positive_side(arealine.point());
			}

			// configure type
			if (ab_determines_shape) {

				auto intersection = CGAL::intersection(arealine, ab);
				edata.point = std::get<Point<K>>(*intersection);

				Segment<K> ns = Segment<K>(edata.point, d);
				auto intersection2 = CGAL::intersection(bc, ns);
				Point<K> is = std::get<Point<K>>(*intersection2);

				edata.T1 = Triangle<K>(b, is, edata.point);
				edata.T2 = Triangle<K>(c, d, is);

			} else {
				auto intersection = CGAL::intersection(arealine, cd);
				edata.point = std::get<Point<K>>(*intersection);

				Segment<K> ns = Segment<K>(edata.point, a);
				auto intersection2 = CGAL::intersection(bc, ns);
				Point<K> is = std::get<Point<K>>(*intersection2);

				edata.T1 = Triangle<K>(a, b, is);
				edata.T2 = Triangle<K>(c, is, edata.point);
			}
		}

		// since it is an area preserving method, T1 and T2 have the same area
		edata.cost = 2 * CGAL::abs(edata.T1.area());
	}
};

template <typename K> using KronenfeldEtAl = EdgeCollapse<KronenfeldEtAlTraits<K>, K>;

} // namespace cartocrow::simplification