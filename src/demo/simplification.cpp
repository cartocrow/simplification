#include "library/point_quad_tree.h"
#include "library/segment_quad_tree.h"
#include "library/straight_graph.h"

#include "library/vertex_removal.h"

#include <iostream>

using namespace cartocrow;
using namespace cartocrow::simplification;

void testVW() {
	std::cout << "started VW test\n";

	using MyKernel = Exact;
	// geometries
	using MyRectangle = Rectangle<MyKernel>;
	using MyPoint = Point<MyKernel>;
	// datastructures
	using MyGraph = VertexRemovalGraph<MyKernel>;
	using MyVertex = MyGraph::Vertex;
	using MyPQT = PointQuadTree<MyVertex, MyKernel>;
	// algorithms
	using MyAlgorithm = VisvalingamWhyatt<MyGraph>;

	MyRectangle box(0, 0, 100, 100);
	MyPQT* pqt = new MyPQT(box, 3);

	MyGraph* g = new MyGraph();

	MyVertex* a = g->addVertex(MyPoint(51, 51));
	MyVertex* b = g->addVertex(MyPoint(10, 40));
	MyVertex* c = g->addVertex(MyPoint(10, 90));
	MyVertex* d = g->addVertex(MyPoint(55, 55));
	MyVertex* e = g->addVertex(MyPoint(90, 10));
	MyVertex* f = g->addVertex(MyPoint(40, 10));
	g->addEdge(a, b);
	g->addEdge(b, c);
	g->addEdge(c, d);
	g->addEdge(d, e);
	g->addEdge(e, f);
	g->addEdge(f, a);

	g->orient();

	assert(g->isOriented());

	std::cout << "PRE " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
		          << "\n";
	}

	MyAlgorithm* vw = new MyAlgorithm(*g, *pqt);
	vw->initialize(true); // true signals that the pqt hasnt been filled yet

	vw->runToComplexity(3);

	std::cout << "POST " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
		          << "\n";
	}

	delete vw;
	delete pqt;
	delete g;

	std::cout << "done VW test\n";
}

void testHistoricVW() {
	std::cout << "started Historic VW test\n";

	using MyKernel = Exact;
	// geometries
	using MyRectangle = Rectangle<MyKernel>;
	using MyPoint = Point<MyKernel>;
	// datastructures
	using MyGraph = HistoricVertexRemovalGraph<MyKernel>;
	using MyBaseGraph = MyGraph::BaseGraph;
	using MyVertex = MyGraph::Vertex;
	using MyPQT = PointQuadTree<MyVertex, MyKernel>;
	// algorithms
	using MyAlgorithm = VisvalingamWhyatt<MyGraph>;

	MyRectangle box(0, 0, 100, 100);
	MyPQT* pqt = new MyPQT(box, 3);

	MyBaseGraph* g = new MyBaseGraph();

	MyVertex* a = g->addVertex(MyPoint(51, 51));
	MyVertex* b = g->addVertex(MyPoint(10, 40));
	MyVertex* c = g->addVertex(MyPoint(10, 90));
	MyVertex* d = g->addVertex(MyPoint(55, 55));
	MyVertex* e = g->addVertex(MyPoint(90, 10));
	MyVertex* f = g->addVertex(MyPoint(40, 10));
	g->addEdge(a, b);
	g->addEdge(b, c);
	g->addEdge(c, d);
	g->addEdge(d, e);
	g->addEdge(e, f);
	g->addEdge(f, a);

	g->orient();

	std::cout << "PRE " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
			<< "\n";
	}

	MyGraph* hg = new MyGraph(*g);

	MyAlgorithm* vw = new MyAlgorithm(*hg, *pqt);
	vw->initialize(true); // true signals that the pqt hasnt been filled yet

	vw->runToComplexity(4);

	std::cout << "POST " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
			<< "\n";
	}

	hg->backInTime();

	std::cout << "BACK " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
			<< "\n";
	}

	// meddling with past, so to continue, reinitialize the algorithm
	hg->goToPresent();
	vw->initialize(true);

	vw->runToComplexity(3);

	std::cout << "POST " << g->getVertexCount() << "\n";
	for (MyVertex* v : g->getVertices()) {
		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
			<< "\n";
	}

	delete vw;
	delete pqt;
	delete hg;
	delete g;

	std::cout << "done VW test\n";
}

//void testKSBB() {
//	std::cout << "started KSBB test\n";
//
//	using MyKernel = Inexact;
//	// geometries
//	using MyRectangle = Rectangle<MyKernel>;
//	using MyPoint = Point<MyKernel>;
//	// datastructures
//	using MyGraph = ECGraph<MyKernel>;
//	using MyVertex = ECVertex<MyKernel>;
//	using MyPQT = PointQuadTree<ECVertex<MyKernel>, MyKernel>;
//	using MySQT = SegmentQuadTree<ECEdge<MyKernel>, MyKernel>;
//	// algorithms
//	using MyAlgorithm = KronenfeldEtAl<MyKernel>;
//
//	MyRectangle box(0, 0, 100, 100);
//	MyPQT* pqt = new MyPQT(box, 3);
//	MySQT* sqt = new MySQT(box, 3, 0.05);
//
//	MyGraph* g = new MyGraph();
//
//	MyVertex* a = g->addVertex(MyPoint(51, 51));
//	MyVertex* b = g->addVertex(MyPoint(10, 40));
//	MyVertex* c = g->addVertex(MyPoint(10, 90));
//	MyVertex* d = g->addVertex(MyPoint(55, 55));
//	MyVertex* e = g->addVertex(MyPoint(90, 10));
//	MyVertex* f = g->addVertex(MyPoint(40, 10));
//	g->addEdge(a, b);
//	g->addEdge(b, c);
//	g->addEdge(c, d);
//	g->addEdge(d, e);
//	g->addEdge(e, f);
//	g->addEdge(f, a);
//
//	std::cout << "PRE " << g->getVertexCount() << "\n";
//	for (MyVertex* v : g->getVertices()) {
//		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
//		          << "\n";
//	}
//
//	MyAlgorithm* ksbb = new MyAlgorithm(*g, *sqt, *pqt);
//
//	ksbb->initialize(true, true); // true signals that the sqt & pqt havent been filled yet
//
//	ksbb->runToComplexity(3);
//
//	std::cout << "POST " << g->getVertexCount() << "\n";
//	for (MyVertex* v : g->getVertices()) {
//		std::cout << " " << v->graphIndex() << ": " << v->getPoint() << ", deg = " << v->degree()
//		          << "\n";
//	}
//
//	delete ksbb;
//	delete pqt;
//	delete g;
//
//	std::cout << "done KSBB test\n";
//}

int main(int argc, char** argv) {

	testVW();

	testHistoricVW();

	//testKSBB();
	
}