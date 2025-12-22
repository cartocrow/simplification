#include "vw.h"

#include "library/vertex_removal.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

using VWGraph = HistoricVertexRemovalGraph<MyKernel>;
using VWPQT = PointQuadTree<VWGraph::Vertex, MyKernel>;
using VW = VisvalingamWhyatt<VWGraph>;

static VWSimplifier* instance = nullptr;
static VWGraph::BaseGraph* m_base = nullptr;
static VWGraph* m_graph = nullptr;
static VWPQT* m_pqt = nullptr;
static VW* m_alg = nullptr;
static SmoothGraph* m_smooth = nullptr;

VWSimplifier& VWSimplifier::getInstance() {
	if (instance == nullptr) {
		instance = new VWSimplifier();
	}
	return *instance;
}

void VWSimplifier::initialize(InputGraph* graph, const int depth) {

	if (hasResult()) {
		clear();
	}

	m_base = copyGraph<InputGraph,VWGraph::BaseGraph>(graph);

	Rectangle<MyKernel> box = utils::boxOf<VWGraph::Vertex,MyKernel>(m_base->getVertices());
	m_pqt = new VWPQT(box, depth);

	m_graph = new VWGraph(*m_base);

	m_alg = new VW(*m_graph, *m_pqt);
	m_alg->initialize(true);
}

void VWSimplifier::runToComplexity(const int k) {
	if (hasResult()) {
		if (hasSmoothResult()) {
			delete m_smooth;
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
					m_alg->initialize(true);
					if (k < m_graph->getEdgeCount()) {
						// still more steps to try
						m_alg->runToComplexity(k);
					}
				}
			}
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

std::shared_ptr<GeometryPainting> VWSimplifier::getPainting(const VertexMode vmode) {
	if (hasResult()) {
		return std::make_shared<GraphPainting<VWGraph>>(*m_graph, Color{ 80, 80, 200 }, 2, vmode);
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

	if (hasSmoothResult()) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}

void VWSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle) {
	if (hasSmoothResult()) {
		delete m_smooth;
	}

	m_smooth = smoothGraph<VWGraph::BaseGraph>(&(m_graph->getBaseGraph()), radius, edges_on_semicircle);
}

bool VWSimplifier::hasSmoothResult() {
	return m_smooth != nullptr;
}

std::shared_ptr<GeometryPainting> VWSimplifier::getSmoothPainting() {
	return std::make_shared<GraphPainting<SmoothGraph>>(*m_smooth, Color{50, 50, 150}, 2, VertexMode::DEG0_ONLY);
}
