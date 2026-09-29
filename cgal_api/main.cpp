#include "straight_simplification_algorithms.h"

#include <cartocrow/renderer/ipe_renderer.h>

int main() {
	using K = CGAL::Epick;
	using Rectangle = CGAL::Iso_rectangle_2<K>;
	using Polygon = CGAL::Polygon_2<CGAL::Epick>;
	using Point = CGAL::Point_2<CGAL::Epick>;

	Rectangle bbox(0, 0, 100, 100);
	CGAL::VW<CGAL::Epick, true> vw(bbox);
	
	Polygon p;
	for (int i = 0; i < 100; ++i) {
		p.push_back(Point(50 + 40 * cos(i / 50.0 * std::numbers::pi), 50 + 40 * sin(i / 50.0 * std::numbers::pi)));
	}

	auto handle = vw.add(p);
	vw.simplify(10);
	auto simplified = vw.get_simplified(handle);

	cartocrow::renderer::IpeRenderer r;
	r.addPainting([&](auto& r) {
		r.draw(simplified);
	});
	r.save("cgal_api_test.ipe");

	return 0;
}