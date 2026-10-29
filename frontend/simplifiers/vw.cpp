#include "vw.h"

#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>

#include <cartocrow/utils/stopwatch.h>

#include "library/visvalingam_whyatt.h"

using namespace cartocrow::simplification;

struct VWSimplifier::State {

	int m_init_complexity;

	virtual ~State() {
	}

	virtual void initialize(std::shared_ptr<InputGraph> graph, const int depth) = 0;
	virtual void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) = 0;
	virtual int getComplexity() = 0;

	int getMaximumComplexity() {
		return m_init_complexity;
	}

	virtual std::shared_ptr<GeometryPainting> getPainting(Color color, const VertexSelection vmode) = 0;
	virtual std::shared_ptr<InputGraph> simplifiedToInputGraph() = 0;
	virtual std::shared_ptr<OutputGraph> simplifiedToOutputGraph() = 0;
};

template <class Alg>
struct VWState : VWSimplifier::State {
	using Graph = Alg::Graph;
	using Kernel = Graph::Kernel;

	std::shared_ptr<Graph> m_graph = nullptr;
	std::unique_ptr<Alg> m_alg = nullptr;

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) override {

		m_graph = std::make_shared<Graph>();

		graph_2_copy(*graph, *m_graph);

		m_alg = std::make_unique<Alg>(*m_graph);
		m_alg->initialize(depth);

		m_init_complexity = m_graph->number_of_edges();
	}

	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) override {
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

	int getComplexity() override {
		return m_graph->number_of_edges();
	}

	std::shared_ptr<GeometryPainting> getPainting(Color color, const VertexSelection vmode) override {
		return std::make_shared<GraphPainting<Graph>>(m_graph, VertexStyle{ color }, EdgeStyle{ color, 2 }, vmode);
	}

	std::shared_ptr<InputGraph> simplifiedToInputGraph() override {
		std::shared_ptr<InputGraph> res = std::make_shared<InputGraph>();
		graph_2_copy(*m_graph, *res);
		return res;
	}

	std::shared_ptr<OutputGraph> simplifiedToOutputGraph() override {
		std::shared_ptr<OutputGraph> res = std::make_shared<OutputGraph>();
		graph_2_copy(*m_graph, *res);
		return res;
	}
};

template <class Kernel>
struct VWState_Old : VWSimplifier::State {
	using Graph = VertexRemovalGraph<Kernel, true>;
	using Alg = VisvalingamWhyatt<Graph>;
	using PQT = Alg::VertexTree;

	std::shared_ptr<Graph> m_graph = nullptr;
	PQT* m_pqt = nullptr;
	Alg* m_alg = nullptr;

	~VWState_Old() {
		delete m_alg;
		delete m_pqt;
	}

	void initialize(std::shared_ptr<InputGraph> graph, const int depth) override {

		m_graph = std::make_shared<Graph>();

		graph_2_copy(*graph, *m_graph);

		Rectangle<Kernel> box = m_graph->bounding_rectangle();
		m_pqt = new PQT(box, depth);

		m_alg = new Alg(*m_graph, *m_pqt);
		m_alg->initialize(true);

		m_init_complexity = m_graph->number_of_edges();
	}

	void runToComplexity(const int k, std::optional<std::function<void(int)>> progress = std::nullopt,
		std::optional<std::function<bool()>> cancelled = std::nullopt) override {
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

	int getComplexity() override {
		return m_graph->number_of_edges();
	}

	std::shared_ptr<GeometryPainting> getPainting(Color color, const VertexSelection vmode) override {
		return std::make_shared<GraphPainting<Graph>>(m_graph, VertexStyle{ color }, EdgeStyle{ color, 2 }, vmode);
	}

	std::shared_ptr<InputGraph> simplifiedToInputGraph() override {
		std::shared_ptr<InputGraph> res = std::make_shared<InputGraph>();
		graph_2_copy(*m_graph, *res);
		return res;
	}

	std::shared_ptr<OutputGraph> simplifiedToOutputGraph() override {
		std::shared_ptr<OutputGraph> res = std::make_shared<OutputGraph>();
		graph_2_copy(*m_graph, *res);
		return res;
	}
};

template <class Kernel>
using VWState_Generic = VWState<VisvalingamWhyatt_Generic<Straight_graph_2<std::monostate, std::monostate, Kernel, DecomposedGraph<true, std::monostate>>>>;

template <class Kernel>
using VWState_Dedicated = VWState<VisvalingamWhyatt_Dedicated<Kernel, true>>;

template <class Kernel>
using VWState2_Generic = VWState<VisvalingamWhyatt_Generic2<Straight_graph_2<std::monostate, std::monostate, Kernel, DecomposedGraph<true, std::monostate>>>>;

template <class Kernel>
using VWState2_Dedicated = VWState<VisvalingamWhyatt_Dedicated2<Kernel, true>>;

template<class Kernel>
using VWState_Old_Dedicated = VWState_Old<VisvalingamWhyatt<VertexRemovalGraph<Kernel, true>>>;


VWSimplifier::VWSimplifier(PersistentSettings& settings) : Simplifier("Visvalingam-Whyatt", settings, { 100, 40, 40 }), m_color(220, 80, 80) {
	m_exact = settings.getBoolean("vw_exact", false);
}

QWidget* VWSimplifier::constructWidget() {
	auto* result = new QWidget();
	auto* layout = new QVBoxLayout(result);
	layout->setAlignment(Qt::AlignTop);

	auto* checkExact = new QCheckBox("Exact computation");
	checkExact->setChecked(m_exact);
	layout->addWidget(checkExact);
	result->connect(checkExact, &QCheckBox::stateChanged, [this](bool checked) {
		m_settings.setBoolean("vw_exact", checked);
		m_exact = checked;
		});

	auto* old = new QCheckBox("old");
	layout->addWidget(old);
	result->connect(old, &QCheckBox::stateChanged, [this](bool checked) {
		m_old = checked;
		});
	auto* generic = new QCheckBox("generic");
	layout->addWidget(generic);
	result->connect(generic, &QCheckBox::stateChanged, [this](bool checked) {
		m_generic = checked;
		});
	auto* v2 = new QCheckBox("v2");
	layout->addWidget(v2);
	result->connect(v2, &QCheckBox::stateChanged, [this](bool checked) {
		m_v2 = checked;
		});

	return result;
}

std::string VWSimplifier::setState() {
	clear();
	if (m_old) {
		if (m_exact) {
			m_state = std::make_unique<VWState_Old<Exact>>();
		}
		else {
			m_state = std::make_unique<VWState_Old<Inexact>>();
		}
		return "old";
	}
	else if (m_v2) {
		if (m_generic) {
			if (m_exact) {
				m_state = std::make_unique<VWState2_Generic<Exact>>();
			}
			else {
				m_state = std::make_unique<VWState2_Generic<Inexact>>();
			}
			return "v2 generic";
		}
		else {
			if (m_exact) {
				m_state = std::make_unique<VWState2_Dedicated<Exact>>();
			}
			else {
				m_state = std::make_unique<VWState2_Dedicated<Inexact>>();
			}
			return "v2 dedicated";
		}
	}
	else if (m_generic) {
		if (m_exact) {
			m_state = std::make_unique<VWState_Generic<Exact>>();
		}
		else {
			m_state = std::make_unique<VWState_Generic<Inexact>>();
		}
		return "v1 generic";
	}
	else {
		if (m_exact) {
			m_state = std::make_unique<VWState_Dedicated<Exact>>();
		}
		else {
			m_state = std::make_unique<VWState_Dedicated<Inexact>>();
		}
		return "v1 dedicated";
	}
}

// TIME TRIAL FOR DEBUGGING PURPOSES
void VWSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	cartocrow::utils::StopwatchPool pool("TimeTrial");

	auto trial = [&pool, graph, depth, this]() {

		auto& watch = pool.get(setState());
		int iterations = 5;
		while (iterations > 0) {

			setState();

			watch.start();
			m_state->initialize(graph, depth);
			m_state->runToComplexity(1);
			watch.stop();

			iterations--;
		}
		};

	m_generic = false;
	m_v2 = false;
	m_old = true;
	trial(); // old
	m_old = false;
	trial(); // v1 ded
	m_generic = true;
	trial(); // v1 gen
	m_v2 = true;
	m_generic = false;
	trial(); // v2 ded
	m_generic = true;
	trial(); // v2 gen	

	pool.printAll();
}

//void VWSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
//	setState();
//	m_state->initialize(graph, depth);
//}

void VWSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	if (hasResult()) {
		clearPostprocessed();
		m_state->runToComplexity(k, progress, cancelled);
	}
}

int VWSimplifier::getComplexity() {
	return hasResult() ? m_state->getComplexity() : -1;
}

int VWSimplifier::getMaximumComplexity() {
	return hasResult() ? m_state->getMaximumComplexity() : -1;
}

std::shared_ptr<GeometryPainting> VWSimplifier::getPainting(const VertexSelection vmode) {
	return hasResult() ? m_state->getPainting(m_color, vmode) : nullptr;
}

void VWSimplifier::clear() {
	clearPostprocessed();
	m_state = nullptr;
}

bool VWSimplifier::hasResult() {
	return m_state != nullptr;
}

std::shared_ptr<InputGraph> VWSimplifier::simplifiedToInputGraph() {
	return hasResult() ? m_state->simplifiedToInputGraph() : nullptr;
}

std::shared_ptr<OutputGraph> VWSimplifier::simplifiedToOutputGraph() {
	return hasResult() ? m_state->simplifiedToOutputGraph() : nullptr;
}