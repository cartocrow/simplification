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
		if (alg->hasSmoothResult()) {
			m_renderer->addPainting(alg->getSmoothPainting(), alg->getName()+" (smooth)");
		}
	}

	m_renderer->update();
}

SimplificationGUI::SimplificationGUI() {
	setWindowTitle("Simplification");

	algorithms.push_back(&VWSimplifier::getInstance());
	algorithms.push_back(&KSBBSimplifier::getInstance());
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

	auto* stepSpinLabel = new QLabel("Step size");
	vLayout->addWidget(stepSpinLabel);
	auto* stepSpin = new QSpinBox();
	stepSpin->setMinimum(1);
	stepSpin->setMaximum(1000);
	stepSpin->setValue(50);
	vLayout->addWidget(stepSpin);

	auto* reverseButton = new QPushButton("Step back");
	vLayout->addWidget(reverseButton);
	auto* stepButton = new QPushButton("Step forward");
	vLayout->addWidget(stepButton);

	auto* runButton = new QPushButton("Run to complexity");
	vLayout->addWidget(runButton);
	desiredComplexity = new QSpinBox();
	desiredComplexity->setValue(10);
	vLayout->addWidget(desiredComplexity);

	complexitySlider = new QSlider();
	complexitySlider->setFocusPolicy(Qt::StrongFocus);
	complexitySlider->setTickPosition(QSlider::TicksBothSides);
	complexitySlider->setTickInterval(250);
	complexitySlider->setSingleStep(stepSpin->value());
	complexitySlider->setOrientation(Qt::Horizontal);
	complexitySlider->setInvertedAppearance(true);
	complexitySlider->setMinimum(0);
	vLayout->addWidget(complexitySlider);


	auto* smoothHeader = new QLabel("<h3>Smoothing</h3>");
	vLayout->addWidget(smoothHeader);

	auto* smoothLabel = new QLabel("Smooth radius: % of sqrt(bbox area)");
	vLayout->addWidget(smoothLabel);
	auto* smoothSlider = new QSlider();
	smoothSlider->setFocusPolicy(Qt::StrongFocus);
	smoothSlider->setTickPosition(QSlider::TicksBothSides);
	smoothSlider->setTickInterval(1000);
	smoothSlider->setSingleStep(1);
	smoothSlider->setOrientation(Qt::Horizontal);
	smoothSlider->setMinimum(1);
	smoothSlider->setMaximum(10000);
	smoothSlider->setValue(500);
	vLayout->addWidget(smoothSlider);

	auto* samplesLabel = new QLabel("Edges on semicircle");
	vLayout->addWidget(samplesLabel);
	auto* samplesSpin = new QSpinBox();
	samplesSpin->setMinimum(1);
	samplesSpin->setMaximum(1000);
	samplesSpin->setValue(90);
	vLayout->addWidget(samplesSpin);

	auto* smoothButton = new QPushButton("Smooth");
	vLayout->addWidget(smoothButton);

	m_renderer = new GeometryWidget();
	m_renderer->setDrawAxes(false);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.01);
	m_renderer->setMaxZoom(1000.0);

	connect(loadFileButton, &QPushButton::clicked, [this, depthSpin]() {
		QString startDir = ".";
		std::filesystem::path filePath = QFileDialog::getOpenFileName(this, tr("Select isolines"), startDir).toStdString();
		if (filePath == "") return;
		loadInput(filePath, depthSpin->value());
		});


	auto smoothChange = [this, algorithmSelector, smoothSlider, samplesSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->smooth(Number<Inexact>(smoothSlider->value() / (double)smoothSlider->maximum()), samplesSpin->value());
			updatePaintings();
		}
		};
	connect(smoothSlider, &QSlider::valueChanged, smoothChange);
	connect(samplesSpin, &QSpinBox::textChanged, smoothChange);
	connect(smoothButton, &QPushButton::clicked, smoothChange);

	connect(initButton, &QPushButton::clicked, [this, algorithmSelector, depthSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (input != nullptr) {
			alg->initialize(input, depthSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			updatePaintings();
		}
		});

	connect(reverseButton, &QPushButton::clicked, [this, algorithmSelector, stepSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() + stepSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(stepButton, &QPushButton::clicked, [this, algorithmSelector, stepSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() - stepSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(runButton, &QPushButton::clicked, [this, algorithmSelector]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(desiredComplexity->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(stepSpin, &QSpinBox::textChanged, [this, stepSpin]() {
		complexitySlider->setSingleStep(stepSpin->value());
		});

	connect(complexitySlider, &QSlider::valueChanged, [this, algorithmSelector](int value) {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(value);
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
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

	input = readIpeFile<InputGraph>(path, depth);

	updatePaintings();

	if (input != nullptr) {
		input->orient();
		desiredComplexity->setMaximum(input->getEdgeCount());
		complexitySlider->setMaximum(input->getEdgeCount());
		m_renderer->fitInView(utils::boxOf<InputGraph::Vertex, MyKernel>(input->getVertices()).bbox());
	}
}
