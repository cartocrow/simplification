#include "pvw.h"

#include <QVBoxLayout>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>

#include "library/preserving_visvalingam_whyatt.h"

using namespace cartocrow::simplification;

struct PVWSimplifier::State {

	int m_init_complexity;

	virtual void initialize(std::shared_ptr<InputGraph> graph, const int depth, const double weight) = 0;
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

template <class Kernel>
struct PVWState : PVWSimplifier::State {
	using Alg = PreservingVisvalingamWhyatt_Dedicated<Kernel, true>;
	using Graph = Alg::Graph;
	using PQT = Alg::VertexTree;
	using Traits = Alg::Score_traits;

	std::shared_ptr<Graph> m_graph = nullptr;
	std::unique_ptr<Alg> m_alg = nullptr;

	void initialize(std::shared_ptr<InputGraph> graph, const int depth, const double weight) override {

		m_graph = std::make_shared<Graph>();

		graph_2_copy(*graph, *m_graph);

		m_alg = std::make_unique<Alg>(*m_graph, Traits(weight));
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

PVWSimplifier::PVWSimplifier(PersistentSettings& settings) : Simplifier("Preserving Visvalingam-Whyatt", settings, { 100, 100, 40 }), m_color(220, 220, 80) {
	m_exact = settings.getBoolean("pvw_exact", false);
	m_preservation_weight = settings.getInteger("pvw_weight", 100);
}

QWidget* PVWSimplifier::constructWidget() {
	auto* result = new QWidget();
	auto* layout = new QVBoxLayout(result);
	layout->setAlignment(Qt::AlignTop);

	auto* checkExact = new QCheckBox("Exact computation");
	checkExact->setChecked(m_exact);
	layout->addWidget(checkExact);

	layout->addWidget(new QLabel("Area-preservation weight (%)"));
	auto* spinWeight = new QSpinBox();
	spinWeight->setMinimum(0);
	spinWeight->setMaximum(1000);
	spinWeight->setValue(m_preservation_weight);
	layout->addWidget(spinWeight);

	result->connect(checkExact, &QCheckBox::stateChanged, [this](bool checked) {
		m_settings.setBoolean("pvw_exact", checked);
		m_exact = checked;
		});

	result->connect(spinWeight, QOverload<int>::of(&QSpinBox::valueChanged), [this](int value) {
		m_settings.setInteger("pvw_weight", value);
		m_preservation_weight = value;		
		});

	return result;
}

void PVWSimplifier::initialize(std::shared_ptr<InputGraph> graph, const int depth) {
	clear();

	if (m_exact) {
		m_state = std::make_unique<PVWState<Exact>>();
	}
	else {
		m_state = std::make_unique<PVWState<Inexact>>();
	}
	m_state->initialize(graph, depth, m_preservation_weight / 100.0);
}

void PVWSimplifier::runToComplexity(const int k, std::optional<std::function<void(int)>> progress,
	std::optional<std::function<bool()>> cancelled) {
	if (hasResult()) {
		clearPostprocessed();
		m_state->runToComplexity(k);
	}
}

int PVWSimplifier::getComplexity() {
	return hasResult() ? m_state->getComplexity() : -1;
}

int PVWSimplifier::getMaximumComplexity() {
	return hasResult() ? m_state->getMaximumComplexity() : -1;
}

std::shared_ptr<GeometryPainting> PVWSimplifier::getPainting(const VertexSelection vmode) {
	return hasResult() ? m_state->getPainting(m_color, vmode) : nullptr;
}

void PVWSimplifier::clear() {
	clearPostprocessed();
	m_state = nullptr;
}

bool PVWSimplifier::hasResult() {
	return m_state != nullptr;
}

std::shared_ptr<InputGraph> PVWSimplifier::simplifiedToInputGraph() {
	return hasResult() ? m_state->simplifiedToInputGraph() : nullptr;
}

std::shared_ptr<OutputGraph> PVWSimplifier::simplifiedToOutputGraph() {
	return hasResult() ? m_state->simplifiedToOutputGraph() : nullptr;
}