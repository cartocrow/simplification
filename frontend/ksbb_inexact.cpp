#include "ksbb_inexact.h"

#include "library/edge_collapse.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

using KSBBGraph = HistoricEdgeCollapseGraph<Inexact>;
using KSBBPQT = PointQuadTree<KSBBGraph::Vertex, Inexact>;
using KSBBSQT = SegmentQuadTree<KSBBGraph::Edge, Inexact>;
using KSBB = KronenfeldEtAl<KSBBGraph>;

static KSBBInexactSimplifier* instance = nullptr;
static KSBBGraph::BaseGraph* m_base = nullptr;
static KSBBGraph* m_graph = nullptr;
static KSBBPQT* m_pqt = nullptr;
static KSBBSQT* m_sqt = nullptr;
static KSBB* m_alg = nullptr;
static SmoothGraph* m_smooth = nullptr;
static bool m_reinit = false;
static int m_init_complexity = -1;

static Color m_color{ 220, 80, 80 };
static Color m_smooth_color = Color{ 100, 40, 40 };

KSBBInexactSimplifier& KSBBInexactSimplifier::getInstance() {
	if (instance == nullptr) {
		instance = new KSBBInexactSimplifier();
	}
	return *instance;
}

void KSBBInexactSimplifier::initialize(InputGraph* graph, const int depth) {

	if (hasResult()) {
		clear();
	}

	copy(graph, m_base);

	Rectangle<Inexact> box = utils::boxOf<KSBBGraph::Vertex, Inexact>(m_base->getVertices());
	m_pqt = new KSBBPQT(box, depth);
	m_sqt = new KSBBSQT(box, depth, 0.05);

	m_graph = new KSBBGraph(*m_base);

	m_alg = new KSBB(*m_graph, *m_sqt, *m_pqt);
	m_alg->initialize(true, true);
	m_reinit = false;

	m_init_complexity = getComplexity();
}

void KSBBInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	if (hasResult()) {
		clearSmoothResult();

		if (k > m_graph->getEdgeCount()) {
			// revert
			m_graph->recallComplexity(k);
			m_reinit = true;
		}
		else if (k < m_graph->getEdgeCount()) {
			if (!m_graph->atPresent()) {
				// first redo known operations
				m_graph->recallComplexity(k);
				m_reinit = true;
			}

			if (m_graph->atPresent() && k < m_graph->getEdgeCount()) {
				// see if there's more to perform
				if (m_reinit) {
					// recallComplexity was invoked, reinitialize algorithm
					m_alg->initialize(true, true);
					m_reinit = false;
				}

				// already at present, run algorithm further
				m_alg->run([&](int complexity, Number<Inexact> cost) {
					if (progress.has_value()) {
						(*progress)(complexity);
					}
					if (cancelled.has_value() && (*cancelled)()) {
						return true;
					}

					return complexity <= k;
					});
			}
		}
	}
}

bool KSBBInexactSimplifier::hasResult() {
	return m_graph != nullptr;
}

int KSBBInexactSimplifier::getComplexity() {
	if (hasResult()) {
		return m_graph->getEdgeCount();
	}
	else {
		return -1;
	}
}

int KSBBInexactSimplifier::getMaximumComplexity() {
	return m_init_complexity;
}

std::shared_ptr<GeometryPainting> KSBBInexactSimplifier::getPainting(const VertexMode vmode) {
	if (hasResult()) {
		return std::make_shared<GraphPainting<KSBBGraph>>(*m_graph, m_color, 2, vmode);
	}
	else {
		return nullptr;
	}
}

void KSBBInexactSimplifier::clear() {
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

	clearSmoothResult();
}

void KSBBInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	clearSmoothResult();

	m_smooth = smoothGraph<KSBBGraph::BaseGraph>(&(m_graph->getBaseGraph()), radius, edges_on_semicircle, progress);
}

bool KSBBInexactSimplifier::hasSmoothResult() {
	return m_smooth != nullptr;
}

std::shared_ptr<GeometryPainting> KSBBInexactSimplifier::getSmoothPainting() {
	return std::make_shared<GraphPainting<SmoothGraph>>(*m_smooth, m_smooth_color, 2, VertexMode::DEG0_ONLY);
}

void KSBBInexactSimplifier::clearSmoothResult() {
	if (m_smooth != nullptr) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}


InputGraph* KSBBInexactSimplifier::resultToGraph() {
	if (m_graph == nullptr) {
		return nullptr;
	}
	else if (m_smooth == nullptr) {
		InputGraph* res;
		copy(m_base, res);
		return res;
	}
	else {
		InputGraph* res;
		copy(m_smooth, res);
		return res;
	}
}