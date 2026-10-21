#include "PVW.h"

#include <cartocrow/data_structures/graph_map_2.h>
#include <cartocrow/renderer/graph_painting.h>

#include "library/preserving_vertex_removal.h"
#include "smoother.h"

using namespace cartocrow::simplification;
using namespace cartocrow::renderer;

template<typename Kernel>
struct PVWBaseSimplifier {
	using PVWGraph = PreservingVertexRemovalGraph<Kernel, true>;
	using PVW = PreservingVisvalingamWhyatt<PVWGraph>;
	using PQT = PVW::VertexTree;
	using Traits = PVW::Traits;

	std::shared_ptr<PVWGraph> m_graph = nullptr;
	PQT* m_pqt = nullptr;
	PVW* m_alg = nullptr;
	std::shared_ptr<SmoothGraph> m_smooth = nullptr;
	int m_init_complexity = -1;
	Color m_color, m_smooth_color;

	PVWBaseSimplifier(Color color, Color smooth_color) : m_color(color), m_smooth_color(smooth_color) {}

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) {

		if (hasResult()) {
			clear();
		}

		m_graph = std::make_shared<PVWGraph>();

		graph_2_copy(*graph, *m_graph);

		Rectangle<Kernel> box = m_graph->bounding_rectangle();
		m_pqt = new PQT(box, depth);

		m_alg = new PVW(*m_graph, *m_pqt, Traits());
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

	std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) {
		if (hasResult()) {
			return std::make_shared<GraphPainting<PVWGraph>>(m_graph, VertexStyle{ m_color }, EdgeStyle{ m_color, 2 }, vmode);
		}
		else {
			return nullptr;
		}
	}

	void clear() {
		if (hasResult()) {

			delete m_alg;
			m_alg = nullptr;

			delete m_pqt;
			m_pqt = nullptr;

			m_graph = nullptr;
		}

		clearSmoothResult();
	}

	void smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
		clearSmoothResult();

		m_smooth = smoothGraph<PVWGraph>(m_graph, radius, edges_on_semicircle, progress);
	}

	bool hasSmoothResult() {
		return m_smooth != nullptr;
	}

	std::shared_ptr<GeometryPainting> getSmoothPainting() {
		return std::make_shared<GraphPainting<SmoothGraph>>(m_smooth, VertexStyle{ m_smooth_color }, EdgeStyle{ m_smooth_color, 2 }, VertexSelection::DEG0_ONLY);
	}

	void clearSmoothResult() {
			m_smooth = nullptr;
	}

	std::shared_ptr<InputGraph> resultToGraph() {
		if (m_graph == nullptr) {
			return nullptr;
		}
		else if (m_smooth == nullptr) {
			std::shared_ptr<InputGraph> res = std::make_shared<InputGraph>();
			graph_2_copy(*m_graph, *res);
			return res;
		}
		else {
			std::shared_ptr<InputGraph> res = std::make_shared<InputGraph>();
			graph_2_copy(*m_smooth, *res);
			return res;
		}
	}
};


using PVWExactBase = PVWBaseSimplifier<Exact>;
static PVWSimplifier* exact_instance = nullptr;
static PVWExactBase* exact_base = nullptr;

PVWSimplifier& PVWSimplifier::getInstance() {
	if (exact_instance == nullptr) {
		exact_instance = new PVWSimplifier();
		exact_base = new PVWExactBase({ 80, 80, 220 }, { 40, 40, 100 });
	}
	return *exact_instance;
}

void PVWSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	exact_base->initialize(graph, depth);
}

void PVWSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	exact_base->runToComplexity(k, progress, cancelled);
}

bool PVWSimplifier::hasResult() {
	return exact_base->hasResult();
}

int PVWSimplifier::getComplexity() {
	return exact_base->getComplexity();
}

int PVWSimplifier::getMaximumComplexity() {
	return exact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> PVWSimplifier::getPainting(const VertexSelection vmode) {
	return exact_base->getPainting(vmode);
}

void PVWSimplifier::clear() {
	exact_base->clear();
}

void PVWSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	exact_base->smooth(radius, edges_on_semicircle, progress);
}

bool PVWSimplifier::hasSmoothResult() {
	return exact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> PVWSimplifier::getSmoothPainting() {
	return exact_base->getSmoothPainting();
}

void PVWSimplifier::clearSmoothResult() {
	exact_base->clearSmoothResult();
}

std::shared_ptr<InputGraph> PVWSimplifier::resultToGraph() {
	return exact_base->resultToGraph();
}


using PVWInexactBase = PVWBaseSimplifier<Inexact>;
static PVWInexactSimplifier* inexact_instance = nullptr;
static PVWInexactBase* inexact_base = nullptr;

PVWInexactSimplifier& PVWInexactSimplifier::getInstance() {
	if (inexact_instance == nullptr) {
		inexact_instance = new PVWInexactSimplifier();
		inexact_base = new PVWInexactBase({ 80, 220, 220 }, { 40, 100, 100 });
	}
	return *inexact_instance;
}

void PVWInexactSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	inexact_base->initialize(graph, depth);
}

void PVWInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	inexact_base->runToComplexity(k, progress, cancelled);
}

bool PVWInexactSimplifier::hasResult() {
	return inexact_base->hasResult();
}

int PVWInexactSimplifier::getComplexity() {
	return inexact_base->getComplexity();
}

int PVWInexactSimplifier::getMaximumComplexity() {
	return inexact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> PVWInexactSimplifier::getPainting(const VertexSelection vmode) {
	return inexact_base->getPainting(vmode);
}

void PVWInexactSimplifier::clear() {
	inexact_base->clear();
}

void PVWInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	inexact_base->smooth(radius, edges_on_semicircle, progress);
}

bool PVWInexactSimplifier::hasSmoothResult() {
	return inexact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> PVWInexactSimplifier::getSmoothPainting() {
	return inexact_base->getSmoothPainting();
}

void PVWInexactSimplifier::clearSmoothResult() {
	inexact_base->clearSmoothResult();
}

std::shared_ptr<InputGraph> PVWInexactSimplifier::resultToGraph() {
	return inexact_base->resultToGraph();
}