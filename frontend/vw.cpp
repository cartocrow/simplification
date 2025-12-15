#include "vw.h"

#include "graph_painter.h"

void VWSimplifier::initialize(InputGraph* graph) {

	if (hasResult()) {
		clear();
	}


	m_base = new VWGraph::BaseGraph();

	std::vector<VWGraph::Vertex*> vtxmap;

	for (InputGraph::Vertex* v : graph->getVertices()) {
		VWGraph::Vertex* ov = m_base->addVertex(v->getPoint());
		vtxmap.push_back(ov);
	}

	for (InputGraph::Edge* e : graph->getEdges()) {
		VWGraph::Vertex* v = vtxmap[e->getSource()->graphIndex()];
		VWGraph::Vertex* w = vtxmap[e->getTarget()->graphIndex()];
		m_base->addEdge(v, w);
	}

	m_base->orient();

	Rectangle<MyKernel> box(0, 0, 100, 100);
	m_pqt = new VWPQT(box, 3);

	m_graph = new VWGraph(*m_base);

	m_alg = new VW(*m_graph, *m_pqt);
	m_alg->initialize(true);
}

void VWSimplifier::runToComplexity(const int k) {
	if (hasResult()) {
		m_graph->recallComplexity(k);

		if (m_graph->getEdgeCount() > k && m_graph->atPresent()) {
			m_alg->runToComplexity(k);
		}
	}
}

bool VWSimplifier::hasResult() {
	return m_graph != nullptr;
}

int VWSimplifier::getComplexity() {
	if (hasResult()) {
		return m_graph->getEdgeCount();
	}
	else {
		return -1;
	}
}

std::shared_ptr<GeometryPainting> VWSimplifier::getPainting() {
	if (hasResult()) {
		return std::make_shared<GraphPainting<VWGraph>>(*m_graph, Color{ 80, 80, 200 }, 2);
	}
	else {
		return nullptr;
	}
}

void VWSimplifier::clear() {
	if (hasResult()) {
		delete m_base;
		m_base = nullptr;

		delete m_graph;
		m_graph = nullptr;

		delete m_alg;
		m_alg = nullptr;

		delete m_pqt;
		m_pqt = nullptr;
	}
}
