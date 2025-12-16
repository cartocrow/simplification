#include "ksbb.h"

#include "graph_painter.h"

void KSBBSimplifier::initialize(InputGraph* graph, const int depth) {

	if (hasResult()) {
		clear();
	}

	m_base = new KSBBGraph::BaseGraph();

	std::vector<KSBBGraph::Vertex*> vtxmap;

	for (InputGraph::Vertex* v : graph->getVertices()) {
		KSBBGraph::Vertex* ov = m_base->addVertex(v->getPoint());
		vtxmap.push_back(ov);
	}

	for (InputGraph::Edge* e : graph->getEdges()) {
		KSBBGraph::Vertex* v = vtxmap[e->getSource()->graphIndex()];
		KSBBGraph::Vertex* w = vtxmap[e->getTarget()->graphIndex()];
		m_base->addEdge(v, w);
	}

	m_base->orient();

	Rectangle<MyKernel> box = utils::boxOf<KSBBGraph::Vertex, MyKernel>(m_base->getVertices());
	m_pqt = new KSBBPQT(box, depth);
	m_sqt = new KSBBSQT(box, depth, 0.05);

	m_graph = new KSBBGraph(*m_base);

	m_alg = new KSBB(*m_graph, *m_sqt, *m_pqt);
	m_alg->initialize(true, true);
}

void KSBBSimplifier::runToComplexity(const int k) {
	if (hasResult()) {
		if (k > m_graph->getEdgeCount()) {
			// revert
			m_graph->recallComplexity(k);
		}
		else if (k < m_graph->getEdgeCount()) {
			if (m_graph->atPresent()) {
				// already at present, run algorithm further
				m_alg->runToComplexity(k);
			}
			else {
				// in the past, go forward
				m_graph->recallComplexity(k);
				if (m_graph->atPresent()) {
					// reached last result, reinit algorithm
					m_alg->initialize(true, true);
					if (k < m_graph->getEdgeCount()) {
						// still more steps to try
						m_alg->runToComplexity(k);
					}
				}
			}
		}
	}
}

bool KSBBSimplifier::hasResult() {
	return m_graph != nullptr;
}

int KSBBSimplifier::getComplexity() {
	if (hasResult()) {
		return m_graph->getEdgeCount();
	}
	else {
		return -1;
	}
}

std::shared_ptr<GeometryPainting> KSBBSimplifier::getPainting(const VertexMode vmode) {
	if (hasResult()) {
		return std::make_shared<GraphPainting<KSBBGraph>>(*m_graph, Color{ 80, 80, 200 }, 2, vmode);
	}
	else {
		return nullptr;
	}
}

void KSBBSimplifier::clear() {
	if (hasResult()) {
		delete m_base;
		m_base = nullptr;

		delete m_graph;
		m_graph = nullptr;

		delete m_alg;
		m_alg = nullptr;

		delete m_pqt;
		m_pqt = nullptr;

		delete m_sqt;
		m_sqt = nullptr;
	}
}
