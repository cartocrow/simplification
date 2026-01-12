#include "ksbb.h"

#include "library/edge_collapse.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

using KSBBGraph = HistoricEdgeCollapseGraph<MyKernel>;
using KSBBPQT = PointQuadTree<KSBBGraph::Vertex, MyKernel>;
using KSBBSQT = SegmentQuadTree<KSBBGraph::Edge, MyKernel>;
using KSBB = KronenfeldEtAl<KSBBGraph>;

static KSBBSimplifier* instance = nullptr;
static KSBBGraph::BaseGraph* m_base = nullptr;
static KSBBGraph* m_graph = nullptr;
static KSBBPQT* m_pqt = nullptr;
static KSBBSQT* m_sqt = nullptr;
static KSBB* m_alg = nullptr;
static SmoothGraph* m_smooth = nullptr;

KSBBSimplifier& KSBBSimplifier::getInstance() {
	if (instance == nullptr) {
		instance = new KSBBSimplifier();
	}
	return *instance;
}

void KSBBSimplifier::initialize(InputGraph* graph, const int depth) {

	if (hasResult()) {
		clear();
	}

	m_base = copyGraph<InputGraph,KSBBGraph::BaseGraph>(graph);

	Rectangle<MyKernel> box = utils::boxOf<KSBBGraph::Vertex, MyKernel>(m_base->getVertices());
	m_pqt = new KSBBPQT(box, depth);
	m_sqt = new KSBBSQT(box, depth, 0.05);

	m_graph = new KSBBGraph(*m_base);

	m_alg = new KSBB(*m_graph, *m_sqt, *m_pqt);
	m_alg->initialize(true, true);
}

void KSBBSimplifier::runToComplexity(const int k) {
	if (hasResult()) {
		if (hasSmoothResult()) {
			delete m_smooth;
			m_smooth = nullptr;
		}

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
		return std::make_shared<GraphPainting<KSBBGraph>>(*m_graph, Color{ 80, 200, 80 }, 2, vmode);
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

	if (hasSmoothResult()) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}

void KSBBSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle) {
	if (hasSmoothResult()) {
		delete m_smooth;
		m_smooth = nullptr;
	}

	m_smooth = smoothGraph<KSBBGraph::BaseGraph>(&(m_graph->getBaseGraph()), radius, edges_on_semicircle);
}

bool KSBBSimplifier::hasSmoothResult() {
	return m_smooth != nullptr;
}

std::shared_ptr<GeometryPainting> KSBBSimplifier::getSmoothPainting() {
	return std::make_shared<GraphPainting<SmoothGraph>>(*m_smooth, Color{ 50, 150, 50 }, 2, VertexMode::DEG0_ONLY);
}

void KSBBSimplifier::clearSmoothResult() {
	if (m_smooth != nullptr) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}


InputGraph* KSBBSimplifier::resultToGraph() {
	if (m_graph == nullptr) {
		return nullptr;
	}
	if (m_smooth == nullptr) {
		return copyGraph<KSBBGraph::BaseGraph,InputGraph>(m_base);
	}
	else {
		return copyExactGraph<SmoothGraph,InputGraph>(m_smooth);
	}
}