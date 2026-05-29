#include "vw_inexact.h"

#include "library/vertex_removal.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

using VWGraph = VertexRemovalGraph<Inexact, true>;
using VWPQT = VertexQuadTree<VWGraph>;
using VW = VisvalingamWhyatt<VWGraph>;

static VWInexactSimplifier* instance = nullptr;
static VWGraph* m_graph = nullptr;
static VWPQT* m_pqt = nullptr;
static VW* m_alg = nullptr;
static SmoothGraph* m_smooth = nullptr;
static int m_init_complexity = -1;

static Color m_color{ 80, 220, 220 };
static Color m_smooth_color = Color{ 40, 100, 100 };

VWInexactSimplifier& VWInexactSimplifier::getInstance() {
	if (instance == nullptr) {
		instance = new VWInexactSimplifier();
	}
	return *instance;
}

void VWInexactSimplifier::initialize(InputGraph* graph, const int depth) {

	if (hasResult()) {
		clear();
	}

	m_graph = new VWGraph();

	std::vector<typename VWGraph::Vertex_handle> map;

	for (typename InputGraph::Vertex* v : graph->getVertices()) {
		map.push_back(m_graph->add_vertex(approximate(v->getPoint())));
	}

	for (typename InputGraph::Edge* e : graph->getEdges()) {
		typename VWGraph::Vertex_handle u = map[e->getSource()->graphIndex()];
		typename VWGraph::Vertex_handle v = map[e->getTarget()->graphIndex()];
		m_graph->add_edge(u, v);
	}

	m_graph->initialize();	

	Rectangle<Inexact> box = m_graph->bounding_rectangle();
	m_pqt = new VWPQT(box, depth);

	m_alg = new VW(*m_graph, *m_pqt);
	m_alg->initialize(true);

	m_init_complexity = m_graph->number_of_edges();
}

void VWInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	if (hasResult()) {
		clearSmoothResult();

		auto& hist = m_graph->history();

		if (k > m_graph->number_of_edges()) {
			// revert
			while (hist.can_undo() && m_graph->number_of_edges() < k) {
				hist.undo();
			}
		}
		else if (k < m_graph->number_of_edges()) {
			while (hist.can_redo() && m_graph->number_of_edges() > k) {
				hist.redo();
			}

			if (!hist.can_redo() && k < m_graph->number_of_edges()) {
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

bool VWInexactSimplifier::hasResult() {
	return m_graph != nullptr;
}

int VWInexactSimplifier::getComplexity() {
	if (hasResult()) {
		return m_graph->number_of_edges();
	}
	else {
		return -1;
	}
}

int VWInexactSimplifier::getMaximumComplexity() {
	return m_init_complexity;
}

std::shared_ptr<GeometryPainting> VWInexactSimplifier::getPainting(const VertexMode vmode) {
	if (hasResult()) {
		return std::make_shared<GraphPainting<VWGraph>>(*m_graph, m_color, 2, vmode);
	}
	else {
		return nullptr;
	}
}

void VWInexactSimplifier::clear() {
	if (hasResult()) {		
		delete m_graph;
		m_graph = nullptr;

		delete m_alg;
		m_alg = nullptr;

		delete m_pqt;
		m_pqt = nullptr;
	}

	clearSmoothResult();
}

void VWInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	clearSmoothResult();

	//m_smooth = smoothGraph<VWGraph::BaseGraph>(&(m_graph->getBaseGraph()), radius, edges_on_semicircle, progress);
}

bool VWInexactSimplifier::hasSmoothResult() {
	return m_smooth != nullptr;
}

std::shared_ptr<GeometryPainting> VWInexactSimplifier::getSmoothPainting() {
	return std::make_shared<OldGraphPainting<SmoothGraph>>(*m_smooth, m_smooth_color, 2, VertexMode::DEG0_ONLY);
}

void VWInexactSimplifier::clearSmoothResult() {
	if (m_smooth != nullptr) {
		delete m_smooth;
		m_smooth = nullptr;
	}
}

InputGraph* VWInexactSimplifier::resultToGraph() {
	return nullptr;
	/*if (m_graph == nullptr) {
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
	}*/
}
