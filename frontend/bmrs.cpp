#include "BMRS.h"

#include <memory>

#include <cartocrow/data_structures/graph_map_2.h>
#include <cartocrow/renderer/graph_painting.h>

#include "library/edge_moves.h"
#include "graph_index_painting.h"
#include "smoother.h"

using namespace cartocrow::simplification;
using namespace cartocrow::renderer;

template<typename G>
struct BMRSDebugPainting : public GeometryPainting {
	using Single = simplification::detail::SingleMove<G>;

public:
	BMRSDebugPainting(std::shared_ptr<G> graph)
		: m_graph(std::move(graph)) {
	}

protected:
	void render(const Single& move, GeometryRenderer& renderer) const {
		if (!move.movable()) {
			return;
		}

		if (move.is_blocked()) {
			renderer.setFill({ 200, 20, 20 });
		}
		else if (move.waiting_index >= 0) {
			renderer.setFill({ 20, 20, 200 });
		}
		else if (!move.contractable()) {
			renderer.setFill({ 20, 200, 200 });
		}
		else {
			renderer.setFill({ 20, 200, 20 });
		}

		renderer.draw(move.swept);
	}

	void paint(GeometryRenderer& renderer) const override {

		renderer.setMode(GeometryRenderer::fill);
		renderer.setFillOpacity(100);

		for (typename G::Edge_const_handle e : m_graph->edges()) {
			auto& edata = e->data();
			render(edata.left, renderer);
			render(edata.right, renderer);
		}
	}

private:
	std::shared_ptr<G> m_graph;
};

template<typename Kernel>
struct BMRSBaseSimplifier {
	using BMRSGraph = EdgeMovesGraph<Kernel, true>;
	using BMRSPQT = VertexQuadTree<BMRSGraph>;
	using BMRSSQT = EdgeQuadTree<BMRSGraph>;
	using BMRS = BuchinEtAl<BMRSGraph>;

	std::shared_ptr<BMRSGraph> m_graph = nullptr;
	BMRSPQT* m_pqt = nullptr;
	BMRSSQT* m_sqt = nullptr;
	BMRS* m_alg = nullptr;
	std::shared_ptr<SmoothGraph> m_smooth = nullptr;
	int m_init_complexity = -1;
	Color m_color, m_smooth_color;

	BMRSBaseSimplifier(Color color, Color smooth_color) : m_color(color), m_smooth_color(smooth_color) {}

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) {

		if (hasResult()) {
			clear();
		}

		m_graph = std::make_shared<BMRSGraph>();

		graph_2_copy(*graph, *m_graph);

		Rectangle<Kernel> box = m_graph->bounding_rectangle();
		m_pqt = new BMRSPQT(box, depth);
		m_sqt = new BMRSSQT(box, depth, 0.05);

		m_alg = new BMRS(*m_graph, *m_sqt, *m_pqt);
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

	std::shared_ptr<GeometryPainting> getPainting(const VertexSelection vmode) {
		if (hasResult()) {
			return std::make_shared<GraphPainting<BMRSGraph>>(m_graph, VertexStyle{ m_color }, EdgeStyle{ m_color, 2 }, vmode);
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

		m_smooth = smoothGraph<BMRSGraph>(m_graph, radius, edges_on_semicircle, progress);
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

	std::vector<std::pair<std::shared_ptr<GeometryPainting>, std::string>> getDebugPaintings() {
		std::vector< std::pair<std::shared_ptr<GeometryPainting>, std::string>> paintings;

		paintings.push_back({ std::make_shared<BMRSDebugPainting<BMRSGraph>>(m_graph), "STATE: Single moves" });
		paintings.push_back({ std::make_shared<GraphIndexPainting<BMRSGraph>>(m_graph, true, true), "STATE: Edge indices" });

		return paintings;
	}
};


using BMRSExactBase = BMRSBaseSimplifier<Exact>;
static BMRSSimplifier* exact_instance = nullptr;
static BMRSExactBase* exact_base = nullptr;

BMRSSimplifier& BMRSSimplifier::getInstance() {
	if (exact_instance == nullptr) {
		exact_instance = new BMRSSimplifier();
		exact_base = new BMRSExactBase({ 80, 220, 80 }, { 40, 100, 40 });
	}
	return *exact_instance;
}

void BMRSSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	exact_base->initialize(graph, depth);
}

void BMRSSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	exact_base->runToComplexity(k, progress, cancelled);
}

bool BMRSSimplifier::hasResult() {
	return exact_base->hasResult();
}

int BMRSSimplifier::getComplexity() {
	return exact_base->getComplexity();
}

int BMRSSimplifier::getMaximumComplexity() {
	return exact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> BMRSSimplifier::getPainting(const VertexSelection vmode) {
	return exact_base->getPainting(vmode);
}

void BMRSSimplifier::clear() {
	exact_base->clear();
}

void BMRSSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	exact_base->smooth(radius, edges_on_semicircle, progress);
}

bool BMRSSimplifier::hasSmoothResult() {
	return exact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> BMRSSimplifier::getSmoothPainting() {
	return exact_base->getSmoothPainting();
}

void BMRSSimplifier::clearSmoothResult() {
	exact_base->clearSmoothResult();
}

std::shared_ptr<InputGraph> BMRSSimplifier::resultToGraph() {
	return exact_base->resultToGraph();
}

std::vector<std::pair<std::shared_ptr<GeometryPainting>, std::string>> BMRSSimplifier::getDebugPaintings() {
	return exact_base->getDebugPaintings();
}


using BMRSInexactBase = BMRSBaseSimplifier<Inexact>;
static BMRSInexactSimplifier* inexact_instance = nullptr;
static BMRSInexactBase* inexact_base = nullptr;

BMRSInexactSimplifier& BMRSInexactSimplifier::getInstance() {
	if (inexact_instance == nullptr) {
		inexact_instance = new BMRSInexactSimplifier();
		inexact_base = new BMRSInexactBase({ 220, 80, 80 }, { 100, 40, 40 });
	}
	return *inexact_instance;
}

void BMRSInexactSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	inexact_base->initialize(graph, depth);
}

void BMRSInexactSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	inexact_base->runToComplexity(k, progress, cancelled);
}

bool BMRSInexactSimplifier::hasResult() {
	return inexact_base->hasResult();
}

int BMRSInexactSimplifier::getComplexity() {
	return inexact_base->getComplexity();
}

int BMRSInexactSimplifier::getMaximumComplexity() {
	return inexact_base->getMaximumComplexity();
}

std::shared_ptr<GeometryPainting> BMRSInexactSimplifier::getPainting(const VertexSelection vmode) {
	return inexact_base->getPainting(vmode);
}

void BMRSInexactSimplifier::clear() {
	inexact_base->clear();
}

void BMRSInexactSimplifier::smooth(Number<Inexact> radius, int edges_on_semicircle, std::optional<std::function<void(std::string, int, int)>> progress) {
	inexact_base->smooth(radius, edges_on_semicircle, progress);
}

bool BMRSInexactSimplifier::hasSmoothResult() {
	return inexact_base->hasSmoothResult();
}

std::shared_ptr<GeometryPainting> BMRSInexactSimplifier::getSmoothPainting() {
	return inexact_base->getSmoothPainting();
}

void BMRSInexactSimplifier::clearSmoothResult() {
	inexact_base->clearSmoothResult();
}

std::shared_ptr<InputGraph> BMRSInexactSimplifier::resultToGraph() {
	return inexact_base->resultToGraph();
}

std::vector<std::pair<std::shared_ptr<GeometryPainting>, std::string>> BMRSInexactSimplifier::getDebugPaintings() {
	return inexact_base->getDebugPaintings();
}