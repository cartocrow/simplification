#include "gui.h"

template<class Graph>
class GraphPainting : public GeometryPainting {
public:
	GraphPainting(Graph& graph, const Color color, const double linewidth) : m_graph(graph), m_color(color), m_linewidth(linewidth) {
	}

protected:
	void paint(GeometryRenderer& renderer) const override {
		renderer.setMode(GeometryRenderer::stroke);

		renderer.setStroke(m_color, m_linewidth);

		for (typename Graph::Edge* e : m_graph.getEdges()) {
			renderer.draw(e->getSegment());
		}
	}

private:
	Graph& m_graph;
	const Color m_color;
	const double m_linewidth;
};


void launchGUI(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SimplificationGUI gui;
	gui.show();
	app.exec();
}

void SimplificationGUI::repaint() {
	m_renderer->clear();

	if (input != nullptr) {
		auto paint = std::make_shared<GraphPainting<InputGraph>>(*input, Color{ 30, 30, 30 }, 1);
		m_renderer->addPainting(paint, "Input");
	}

	if (output != nullptr) {
		auto paint = std::make_shared<GraphPainting<OutputGraph>>(*output, Color{ 80, 80, 200 }, 2);
		m_renderer->addPainting(paint, "Output");
	}

	m_renderer->update();
}

void SimplificationGUI::initialize() {
	if (input == nullptr) {
		return;
	}

	if (vw != nullptr) {
		delete vw;
	}
	if (output != nullptr) {
		delete output;
	}

	output = new OutputGraph();

	std::vector<OutputGraph::Vertex*> vtxmap;

	for (InputGraph::Vertex* v : input->getVertices()) {
		OutputGraph::Vertex* ov = output->addVertex(v->getPoint());
		vtxmap.push_back(ov);
	}

	for (InputGraph::Edge* e : input->getEdges()) {
		OutputGraph::Vertex* v = vtxmap[e->getSource()->graphIndex()];
		OutputGraph::Vertex* w = vtxmap[e->getTarget()->graphIndex()];
		output->addEdge(v, w);
	}

	output->orient();

	Rectangle<Inexact> box(0, 0, 100, 100);
	outpqt = new OutputPQT(box, 3);

	vw = new VisvalingamWhyatt<OutputGraph>(*output, *outpqt);
	vw->initialize(true); 
	vw->runToComplexity(3);

	repaint();
}

SimplificationGUI::SimplificationGUI() {
	setWindowTitle("Simplification");

	auto* dockWidget = new QDockWidget();
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
	auto* vWidget = new QWidget();
	auto* vLayout = new QVBoxLayout(vWidget);
	vLayout->setAlignment(Qt::AlignTop);
	dockWidget->setWidget(vWidget);

	auto* basicOptions = new QLabel("<h3>Basic options</h3>");
	vLayout->addWidget(basicOptions);
	auto* directorySelector = new QPushButton("Select input directory");
	vLayout->addWidget(directorySelector);
	auto* fileSelector = new QComboBox();
	auto* fileSelectorLabel = new QLabel("Input file");
	fileSelectorLabel->setBuddy(fileSelector);
	vLayout->addWidget(fileSelectorLabel);
	vLayout->addWidget(fileSelector);

	m_renderer = new GeometryWidget();
	m_renderer->setDrawAxes(false);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.01);
	m_renderer->setMaxZoom(1000.0);

	input = new InputGraph();

	InputGraph::Vertex* a = input->addVertex(Point<Inexact>(51, 51));
	InputGraph::Vertex* b = input->addVertex(Point<Inexact>(10, 40));
	InputGraph::Vertex* c = input->addVertex(Point<Inexact>(10, 90));
	InputGraph::Vertex* d = input->addVertex(Point<Inexact>(55, 55));
	InputGraph::Vertex* e = input->addVertex(Point<Inexact>(90, 10));
	InputGraph::Vertex* f = input->addVertex(Point<Inexact>(40, 10));
	input->addEdge(a, b);
	input->addEdge(b, c);
	input->addEdge(c, d);
	input->addEdge(d, e);
	input->addEdge(e, f);
	input->addEdge(f, a);

	input->orient();

	initialize();
}

SimplificationGUI::~SimplificationGUI() {
	if (m_renderer != nullptr) {
		delete m_renderer;
	}
	if (vw != nullptr) {
		delete vw;
	}
	if (outpqt != nullptr) {
		delete outpqt;
	}
	if (output != nullptr) {
		delete output;
	}
	if (input != nullptr) {
		delete input;
	}
}
