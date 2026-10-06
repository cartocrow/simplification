#include "simplification_vw.h"

#include <cartocrow/renderer/ipe_renderer.h>
#include <cartocrow/core/straight_geometry_collection.h>

using namespace cartocrow;

int main() {
	using K = CGAL::Epick;
	using Rectangle = CGAL::Iso_rectangle_2<K>;
	using Polygon = CGAL::Polygon_2<CGAL::Epick>;
	using Point = CGAL::Point_2<CGAL::Epick>;
	
	Polygon p;
	for (int i = 0; i < 100; ++i) {
		p.push_back(Point(50 + 40 * cos(i / 50.0 * std::numbers::pi), 50 + 40 * sin(i / 50.0 * std::numbers::pi)));
	}

	StraightGeometryCollection<K, std::string> shapes;
	auto pH = shapes.insert(p, "circle polygon");
	//StraightGeometryCollection<K> shapes;
	//auto pH = shapes.insert(p);
	CGAL::Simplification_VW vw(shapes);
	vw.simplify(10);
	auto simplified = vw.get_collection();

	auto simplifiedP = CGAL::Simplification_VW<>::simplify(p, 20);

	cartocrow::renderer::IpeRenderer r;
	r.addPainting([&](auto& r) {
		r.draw(shapes.get_geometry(pH));
		r.draw(simplified.get_geometry(pH));
		r.draw(simplifiedP);
	});
	r.save("cgal_api_test.ipe");

	return 0;
}