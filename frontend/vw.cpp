#include "vw.h"

#include "library/vertex_removal.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

using VWGraph = HistoricVertexRemovalGraph<Exact>;
using VWPQT = PointQuadTree<VWGraph::Vertex, Exact>;
using VW = VisvalingamWhyatt<VWGraph>;

static VWSimplifier* instance = nullptr;
static VWGraph::BaseGraph* m_base = nullptr;
static VWGraph* m_graph = nullptr;
static VWPQT* m_pqt = nullptr;
static VW* m_alg = nullptr;
static SmoothGraph* m_smooth = nullptr;
static bool m_reinit = false;

static Color m_color{ 80, 80, 220 };
static Color m_smooth_color = Color{ 40, 40, 100 };

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

	copy(graph, m_base);

	Rectangle<Exact> box = utils::boxOf<VWGraph::Vertex, Exact>(m_base->getVertices());
	m_pqt = new VWPQT(box, depth);

	m_graph = new VWGraph(*m_base);

	m_alg = new VW(*m_graph, *m_pqt);
	m_alg->initialize(true);
	m_reinit = false;
}

void VWSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
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
					m_alg->initialize(true);
					m_reinit = false;
				}

				// already at present, run algorithm further
				m_alg->run([&](int complexity, Number<Exact> cost) {
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
		return std::make_shared<GraphPainting<VWGraph>>(*m_graph, m_color, 2, vmode);
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

	clearSmoothResult();
}

void VWSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	clearSmoothResult();

	m_smooth = smoothGraph<VWGraph::BaseGraph>(&(m_graph->getBaseGraph()), radius, edges_on_semicircle, progress);
}

bool VWSimplifier::hasSmoothResult() {
	return m_smooth != nullptr;
}

std::shared_ptr<GeometryPainting> VWSimplifier::getSmoothPainting() {
	return std::make_shared<GraphPainting<SmoothGraph>>(*m_smooth, m_smooth_color, 2, VertexMode::DEG0_ONLY);
}

void VWSimplifier::clearSmoothResult() {
	if (m_smooth != nullptr) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}

InputGraph* VWSimplifier::resultToGraph() {
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
