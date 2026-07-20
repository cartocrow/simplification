#include "ksbb.h"

#include <cartocrow/data_structures/graph_map_2.h>

#include "library/edge_collapse.h"
#include "graph_painter.h"
#include "smoother.h"

using namespace cartocrow::simplification;

template<typename Kernel, bool A>
struct KSBBBaseSimplifier {
	using KSBBGraph = EdgeCollapseGraph<Kernel, true>;
	using KSBBPQT = VertexQuadTree<KSBBGraph>;
	using KSBBSQT = EdgeQuadTree<KSBBGraph>;
	using KSBB = EdgeCollapse<KronenfeldEtAlTraits<KSBBGraph, A>>;

	KSBBGraph* m_graph = nullptr;
	KSBBPQT* m_pqt = nullptr;
	KSBBSQT* m_sqt = nullptr;
	KSBB* m_alg = nullptr;
	SmoothGraph* m_smooth = nullptr;
	int m_init_complexity = -1;
	Color m_color, m_smooth_color;

	KSBBBaseSimplifier(Color color, Color smooth_color) : m_color(color), m_smooth_color(smooth_color) {}

	void initialize(InputGraph* graph, const int depth) {

		if (hasResult()) {
			clear();
		}

		m_graph = new KSBBGraph();

		graph_2_copy(*graph, *m_graph);

		Rectangle<Kernel> box = m_graph->bounding_rectangle();
		m_pqt = new KSBBPQT(box, depth);
		m_sqt = new KSBBSQT(box, depth, 0.05);

		m_alg = new KSBB(*m_graph, *m_sqt, *m_pqt);
		m_alg->initialize(true, true);

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
			return std::make_shared<GraphPainting<KSBBGraph>>(*m_graph, m_color, 2, vmode);
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

		m_smooth = smoothGraph<KSBBGraph>(m_graph, radius, edges_on_semicircle, progress);
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


using KSBBExactBase = KSBBBaseSimplifier<Exact,false>;
static KSBBSimplifier* exact_instance = nullptr;
static KSBBExactBase* exact_base = nullptr;

KSBBSimplifier& KSBBSimplifier::getInstance() {
	if (exact_instance == nullptr) {
		exact_instance = new KSBBSimplifier();
		exact_base = new KSBBExactBase({ 80, 220, 80 }, { 40, 100, 40 });
	}
	return *exact_instance;
}

void KSBBSimplifier::initialize(InputGraph* graph, const int depth) {
	exact_base->initialize(graph, depth);
}

void KSBBSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	exact_base->runToComplexity(k, progress, cancelled);
}

bool KSBBSimplifier::hasResult() {
	return exact_base->hasResult();
}

int KSBBSimplifier::getComplexity() {
	return exact_base->getComplexity();
}

int KSBBSimplifier::getMaximumComplexity() {
	return exact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> KSBBSimplifier::getPainting(const VertexMode vmode) {
	return exact_base->getPainting(vmode);
}

void KSBBSimplifier::clear() {
	exact_base->clear();
}

void KSBBSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	exact_base->smooth(radius, edges_on_semicircle, progress);
}

bool KSBBSimplifier::hasSmoothResult() {
	return exact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> KSBBSimplifier::getSmoothPainting() {
	return exact_base->getSmoothPainting();
}

void KSBBSimplifier::clearSmoothResult() {
	exact_base->clearSmoothResult();
}

InputGraph* KSBBSimplifier::resultToGraph() {
	return exact_base->resultToGraph();
}


using KSBBInexactBase = KSBBBaseSimplifier<Inexact,false>;
static KSBBInexactSimplifier* inexact_instance = nullptr;
static KSBBInexactBase* inexact_base = nullptr;

KSBBInexactSimplifier& KSBBInexactSimplifier::getInstance() {
	if (inexact_instance == nullptr) {
		inexact_instance = new KSBBInexactSimplifier();
		inexact_base = new KSBBInexactBase({ 220, 80, 80 }, { 100, 40, 40 });
	}
	return *inexact_instance;
}

void KSBBInexactSimplifier::initialize(InputGraph* graph, const int depth) {
	inexact_base->initialize(graph, depth);
}

void KSBBInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	inexact_base->runToComplexity(k, progress, cancelled);
}

bool KSBBInexactSimplifier::hasResult() {
	return inexact_base->hasResult();
}

int KSBBInexactSimplifier::getComplexity() {
	return inexact_base->getComplexity();
}

int KSBBInexactSimplifier::getMaximumComplexity() {
	return inexact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> KSBBInexactSimplifier::getPainting(const VertexMode vmode) {
	return inexact_base->getPainting(vmode);
}

void KSBBInexactSimplifier::clear() {
	inexact_base->clear();
}

void KSBBInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	inexact_base->smooth(radius, edges_on_semicircle, progress);
}

bool KSBBInexactSimplifier::hasSmoothResult() {
	return inexact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> KSBBInexactSimplifier::getSmoothPainting() {
	return inexact_base->getSmoothPainting();
}

void KSBBInexactSimplifier::clearSmoothResult() {
	inexact_base->clearSmoothResult();
}

InputGraph* KSBBInexactSimplifier::resultToGraph() {
	return inexact_base->resultToGraph();
}


using KSBBSemiExactBase = KSBBBaseSimplifier<Exact, true>;
static KSBBSemiExactSimplifier* semiexact_instance = nullptr;
static KSBBSemiExactBase* semiexact_base = nullptr;

KSBBSemiExactSimplifier& KSBBSemiExactSimplifier::getInstance() {
	if (semiexact_instance == nullptr) {
		semiexact_instance = new KSBBSemiExactSimplifier();
		semiexact_base = new KSBBSemiExactBase({ 220, 220, 80 }, { 100, 100, 40 });
	}
	return *semiexact_instance;
}

void KSBBSemiExactSimplifier::initialize(InputGraph* graph, const int depth) {
	semiexact_base->initialize(graph, depth);
}

void KSBBSemiExactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	semiexact_base->runToComplexity(k, progress, cancelled);
}

bool KSBBSemiExactSimplifier::hasResult() {
	return semiexact_base->hasResult();
}

int KSBBSemiExactSimplifier::getComplexity() {
	return semiexact_base->getComplexity();
}

int KSBBSemiExactSimplifier::getMaximumComplexity() {
	return semiexact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> KSBBSemiExactSimplifier::getPainting(const VertexMode vmode) {
	return semiexact_base->getPainting(vmode);
}

void KSBBSemiExactSimplifier::clear() {
	semiexact_base->clear();
}

void KSBBSemiExactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	semiexact_base->smooth(radius, edges_on_semicircle, progress);
}

bool KSBBSemiExactSimplifier::hasSmoothResult() {
	return semiexact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> KSBBSemiExactSimplifier::getSmoothPainting() {
	return semiexact_base->getSmoothPainting();
}

void KSBBSemiExactSimplifier::clearSmoothResult() {
	semiexact_base->clearSmoothResult();
}

InputGraph* KSBBSemiExactSimplifier::resultToGraph() {
	return semiexact_base->resultToGraph();
}