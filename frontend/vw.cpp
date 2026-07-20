#include "vw.h"

#include <cartocrow/data_structures/graph_map_2.h>

#include "library/vertex_removal.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

template<typename Kernel>
struct VWBaseSimplifier {
	using VWGraph = VertexRemovalGraph<Kernel, true>;
	using VWPQT = VertexQuadTree<VWGraph>;
	using VW = VisvalingamWhyatt<VWGraph>;

	VWGraph* m_graph = nullptr;
	VWPQT* m_pqt = nullptr;
	VW* m_alg = nullptr;
	SmoothGraph* m_smooth = nullptr;
	int m_init_complexity = -1;
	Color m_color, m_smooth_color;

	VWBaseSimplifier(Color color, Color smooth_color) : m_color(color), m_smooth_color(smooth_color) {}

	void initialize(InputGraph* graph, const int depth) {

		if (hasResult()) {
			clear();
		}

		m_graph = new VWGraph();

		graph_2_copy(*graph, *m_graph);

		Rectangle<Kernel> box = m_graph->bounding_rectangle();
		m_pqt = new VWPQT(box, depth);

		m_alg = new VW(*m_graph, *m_pqt);
		m_alg->initialize(true);

		m_init_complexity = m_graph->number_of_edges();
	}

	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
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
					m_alg->run([&](int complexity, Number<Kernel> cost) {
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

	bool hasResult() {
		return m_graph != nullptr;
	}

	int getComplexity() {
		if (hasResult()) {
			return m_graph->number_of_edges();
		}
		else {
			return -1;
		}
	}

	int getMaximumComplexity() {
		return m_init_complexity;
	}

	std::shared_ptr<GeometryPainting> getPainting(const VertexMode vmode) {
		if (hasResult()) {
			return std::make_shared<GraphPainting<VWGraph>>(*m_graph, m_color, 2, vmode);
		}
		else {
			return nullptr;
		}
	}

	void clear() {
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

	void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
		clearSmoothResult();

		m_smooth = smoothGraph<VWGraph>(m_graph, radius, edges_on_semicircle, progress);
	}

	bool hasSmoothResult() {
		return m_smooth != nullptr;
	}

	std::shared_ptr<GeometryPainting> getSmoothPainting() {
		return std::make_shared<GraphPainting<SmoothGraph>>(*m_smooth, m_smooth_color, 2, VertexMode::DEG0_ONLY);
	}

	void clearSmoothResult() {
		if (m_smooth != nullptr) {
			delete m_smooth;
			m_smooth = nullptr;
		}
	}

	InputGraph* resultToGraph() {
		if (m_graph == nullptr) {
			return nullptr;
		}
		else if (m_smooth == nullptr) {
			InputGraph* res = new InputGraph();
			graph_2_copy(*m_graph, *res);
			return res;
		}
		else {
			InputGraph* res = new InputGraph();
			graph_2_copy(*m_smooth, *res);
			return res;
		}
	}
};


using VWExactBase = VWBaseSimplifier<Exact>;
static VWSimplifier* exact_instance = nullptr;
static VWExactBase* exact_base = nullptr;

VWSimplifier& VWSimplifier::getInstance() {
	if (exact_instance == nullptr) {
		exact_instance = new VWSimplifier();
		exact_base = new VWExactBase({ 80, 80, 220 }, { 40, 40, 100 });
	}
	return *exact_instance;
}

void VWSimplifier::initialize(InputGraph* graph, const int depth) {
	exact_base->initialize(graph, depth);
}

void VWSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	exact_base->runToComplexity(k, progress, cancelled);
}

bool VWSimplifier::hasResult() {
	return exact_base->hasResult();
}

int VWSimplifier::getComplexity() {
	return exact_base->getComplexity();
}

int VWSimplifier::getMaximumComplexity() {
	return exact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> VWSimplifier::getPainting(const VertexMode vmode) {
	return exact_base->getPainting(vmode);
}

void VWSimplifier::clear() {
	exact_base->clear();
}

void VWSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	exact_base->smooth(radius, edges_on_semicircle, progress);
}

bool VWSimplifier::hasSmoothResult() {
	return exact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> VWSimplifier::getSmoothPainting() {
	return exact_base->getSmoothPainting();
}

void VWSimplifier::clearSmoothResult() {
	exact_base->clearSmoothResult();
}

InputGraph* VWSimplifier::resultToGraph() {
	return exact_base->resultToGraph();
}


using VWInexactBase = VWBaseSimplifier<Inexact>;
static VWInexactSimplifier* inexact_instance = nullptr;
static VWInexactBase* inexact_base = nullptr;

VWInexactSimplifier& VWInexactSimplifier::getInstance() {
	if (inexact_instance == nullptr) {
		inexact_instance = new VWInexactSimplifier();
		inexact_base = new VWInexactBase({ 80, 220, 220 }, { 40, 100, 100 });
	}
	return *inexact_instance;
}

void VWInexactSimplifier::initialize(InputGraph* graph, const int depth) {
	inexact_base->initialize(graph, depth);
}

void VWInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	inexact_base->runToComplexity(k, progress, cancelled);
}

bool VWInexactSimplifier::hasResult() {
	return inexact_base->hasResult();
}

int VWInexactSimplifier::getComplexity() {
	return inexact_base->getComplexity();
}

int VWInexactSimplifier::getMaximumComplexity() {
	return inexact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> VWInexactSimplifier::getPainting(const VertexMode vmode) {
	return inexact_base->getPainting(vmode);
}

void VWInexactSimplifier::clear() {
	inexact_base->clear();
}

void VWInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	inexact_base->smooth(radius, edges_on_semicircle, progress);
}

bool VWInexactSimplifier::hasSmoothResult() {
	return inexact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> VWInexactSimplifier::getSmoothPainting() {
	return inexact_base->getSmoothPainting();
}

void VWInexactSimplifier::clearSmoothResult() {
	inexact_base->clearSmoothResult();
}

InputGraph* VWInexactSimplifier::resultToGraph() {
	return inexact_base->resultToGraph();
}