#include "gui.h"

#include <QCheckBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

#include "library/utils.h"

#include "vw.h"
#include "ksbb.h"
#include "graph_painter.h"
#include "ipe_reader.h"

void launchGUI(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SimplificationGUI gui;
	gui.show();
	app.exec();
}

void SimplificationGUI::updatePaintings() {
	m_renderer->clear();

	VertexMode vmode = static_cast<VertexMode>(vertexMode->currentIndex());

	if (input != nullptr) {
		auto paint = std::make_shared<GraphPainting<InputGraph>>(*input, Color{ 30, 30, 30 }, 1, vmode);
		m_renderer->addPainting(paint, "Input");
	}

	for (SimplificationAlgorithm* alg : algorithms) {
		if (alg->hasResult()) {
			m_renderer->addPainting(alg->getPainting(vmode), alg->getName());
		}
	}

	m_renderer->update();
}

SimplificationGUI::SimplificationGUI() {
	setWindowTitle("Simplification");

	algorithms.push_back(new VWSimplifier());
	algorithms.push_back(new KSBBSimplifier());
	int default_alg = 1;

	auto* dockWidget = new QDockWidget();
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
	auto* vWidget = new QWidget();
	auto* vLayout = new QVBoxLayout(vWidget);
	vLayout->setAlignment(Qt::AlignTop);
	dockWidget->setWidget(vWidget);

	auto* ioSettings = new QLabel("<h3>Input / output</h3>");
	vLayout->addWidget(ioSettings);

	auto* loadFileButton = new QPushButton("Load file");
	vLayout->addWidget(loadFileButton);

	vertexMode = new QComboBox();
	vertexMode->addItem("Only degree-0 vertices");
	vertexMode->addItem("No degree-2 vertices");
	vertexMode->addItem("All vertices");
	vertexMode->setCurrentIndex(0);
	vLayout->addWidget(vertexMode);

	auto* vwSettings = new QLabel("<h3>Simplification</h3>");
	vLayout->addWidget(vwSettings);

	auto* algorithmSelector = new QComboBox();
	vLayout->addWidget(algorithmSelector);
	for (SimplificationAlgorithm* alg : algorithms) {
		algorithmSelector->addItem(QString::fromStdString(alg->getName()));
	}
	algorithmSelector->setCurrentIndex(default_alg);

	auto* depthSpinLabel = new QLabel("Search-tree depths");
	vLayout->addWidget(depthSpinLabel);
	auto* depthSpin = new QSpinBox();
	depthSpin->setMinimum(1);
	depthSpin->setMaximum(20);
	depthSpin->setValue(10);
	vLayout->addWidget(depthSpin);

	auto* initButton = new QPushButton("Initialize");
	vLayout->addWidget(initButton);

	auto* reverseButton = new QPushButton("Step back");
	vLayout->addWidget(reverseButton);
	auto* stepButton = new QPushButton("Step forward");
	vLayout->addWidget(stepButton);

	auto* runButton = new QPushButton("Run to complexity");
	vLayout->addWidget(runButton);
	desiredComplexity = new QSpinBox();
	desiredComplexity->setValue(10);
	vLayout->addWidget(desiredComplexity);

	m_renderer = new GeometryWidget();
	m_renderer->setDrawAxes(false);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.01);
	m_renderer->setMaxZoom(1000.0);

	input = new InputGraph();

	InputGraph::Vertex* a = input->addVertex(Point<MyKernel>(51, 51));
	InputGraph::Vertex* b = input->addVertex(Point<MyKernel>(10, 40));
	InputGraph::Vertex* c = input->addVertex(Point<MyKernel>(10, 90));
	InputGraph::Vertex* d = input->addVertex(Point<MyKernel>(55, 55));
	InputGraph::Vertex* e = input->addVertex(Point<MyKernel>(90, 10));
	InputGraph::Vertex* f = input->addVertex(Point<MyKernel>(40, 10));
	input->addEdge(a, b);
	input->addEdge(b, c);
	input->addEdge(c, d);
	input->addEdge(d, e);
	input->addEdge(e, f);
	input->addEdge(f, a);

	input->orient();

	connect(loadFileButton, &QPushButton::clicked, [this, depthSpin]() {
		QString startDir = ".";
		std::filesystem::path filePath = QFileDialog::getOpenFileName(this, tr("Select isolines"), startDir).toStdString();
		if (filePath == "") return;
		loadInput(filePath, depthSpin->value());
		});

	connect(initButton, &QPushButton::clicked, [this, algorithmSelector, depthSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (input != nullptr) {
			alg->initialize(input, depthSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			updatePaintings();
		}
		});

	connect(reverseButton, &QPushButton::clicked, [this, algorithmSelector]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() + 1);
			desiredComplexity->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(stepButton, &QPushButton::clicked, [this, algorithmSelector]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() - 1);
			desiredComplexity->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(runButton, &QPushButton::clicked, [this, algorithmSelector]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(desiredComplexity->value());
			desiredComplexity->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(vertexMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int index) {
			updatePaintings();
		});

	updatePaintings();
}

SimplificationGUI::~SimplificationGUI() {
	for (SimplificationAlgorithm* alg : algorithms) {
		alg->clear();
		delete alg;
	}
	if (m_renderer != nullptr) {
		delete m_renderer;
	}
	if (input != nullptr) {
		delete input;
	}
}

void SimplificationGUI::loadInput(const std::filesystem::path& path, const int depth) {
	if (input != nullptr) {
		delete input;

		for (SimplificationAlgorithm* alg : algorithms) {
			alg->clear();
		}
	}

	input = readIpeFile(path, depth);

	updatePaintings();

	if (input != nullptr) {
		desiredComplexity->setMaximum(input->getEdgeCount());
		m_renderer->fitInView(utils::boxOf<InputGraph::Vertex, MyKernel>(input->getVertices()).bbox());
	}
}
