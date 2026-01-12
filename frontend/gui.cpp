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
#include "read_graph_gdal.h"

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
			m_renderer->addPainting(alg->getSmoothPainting(), alg->getName() + " (smooth)");
		}
	}

	m_renderer->update();
}

void SimplificationGUI::addInputTab() {
	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Input"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Input</h3>"));

	auto* loadFileButton = new QPushButton("Load file");
	layout->addWidget(loadFileButton);

	auto* txt = new QLabel("<p>Currently supported file formats:</p><ul><li>IPE files (*.ipe) containing polygons, polylines and points.</li><li>Shapefiles (*.shp) containing (multi)polygons with holes.</li>");
	txt->setWordWrap(true);
	layout->addWidget(txt);


	connect(loadFileButton, &QPushButton::clicked, [this]() {
		std::filesystem::path filePath = QFileDialog::getOpenFileName(this, tr("Select map"), curr_dir, tr("Accepted formats (*.shp *.ipe);;IPE files (*.ipe);;SHP files (*.shp)")).toStdString();
		if (filePath == "") return;
		curr_dir = QString::fromStdU16String(filePath.parent_path().u16string());
		loadInput(filePath, this->depthSpin->value());
		});
}

void SimplificationGUI::addPreprocessTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Preprocess"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Preprocess</h3>"));
}

void SimplificationGUI::addSimplifyTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Simplify"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Simplify</h3>"));

	int default_alg = 1; // KSBB
	algorithmSelector = new QComboBox();
	layout->addWidget(algorithmSelector);
	for (SimplificationAlgorithm* alg : algorithms) {
		algorithmSelector->addItem(QString::fromStdString(alg->getName()));
	}
	algorithmSelector->setCurrentIndex(default_alg);

	auto* initButton = new QPushButton("Initialize");
	layout->addWidget(initButton);

	auto* stepSpinLabel = new QLabel("Step size");
	layout->addWidget(stepSpinLabel);
	auto* stepSpin = new QSpinBox();
	stepSpin->setMinimum(1);
	stepSpin->setMaximum(1000);
	stepSpin->setValue(50);
	layout->addWidget(stepSpin);

	auto* reverseButton = new QPushButton("Step back");
	layout->addWidget(reverseButton);
	auto* stepButton = new QPushButton("Step forward");
	layout->addWidget(stepButton);

	auto* runButton = new QPushButton("Run to complexity");
	layout->addWidget(runButton);
	desiredComplexity = new QSpinBox();
	desiredComplexity->setValue(10);
	layout->addWidget(desiredComplexity);

	complexitySlider = new QSlider();
	complexitySlider->setFocusPolicy(Qt::StrongFocus);
	complexitySlider->setTickPosition(QSlider::TicksBothSides);
	complexitySlider->setTickInterval(250);
	complexitySlider->setSingleStep(stepSpin->value());
	complexitySlider->setOrientation(Qt::Horizontal);
	complexitySlider->setInvertedAppearance(true);
	complexitySlider->setMinimum(0);
	layout->addWidget(complexitySlider);

	connect(initButton, &QPushButton::clicked, [this]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (input != nullptr) {
			alg->initialize(input, depthSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			updatePaintings();
		}
		});

	connect(reverseButton, &QPushButton::clicked, [this, stepSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() + stepSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(stepButton, &QPushButton::clicked, [this, stepSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(alg->getComplexity() - stepSpin->value());
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});

	connect(runButton, &QPushButton::clicked, [this]() {
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

	connect(complexitySlider, &QSlider::valueChanged, [this](int value) {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->runToComplexity(value);
			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			m_renderer->repaint();
		}
		});
}

void SimplificationGUI::addPostprocessTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Postprocess"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Postprocess</h3>"));

	auto* smoothLabel = new QLabel("Smooth radius: % of sqrt(bbox area)");
	layout->addWidget(smoothLabel);
	auto* smoothSlider = new QSlider();
	smoothSlider->setFocusPolicy(Qt::StrongFocus);
	smoothSlider->setTickPosition(QSlider::TicksBothSides);
	smoothSlider->setTickInterval(1000);
	smoothSlider->setSingleStep(1);
	smoothSlider->setOrientation(Qt::Horizontal);
	smoothSlider->setMinimum(1);
	smoothSlider->setMaximum(10000);
	smoothSlider->setValue(500);
	layout->addWidget(smoothSlider);

	auto* samplesLabel = new QLabel("Edges on semicircle");
	layout->addWidget(samplesLabel);
	auto* samplesSpin = new QSpinBox();
	samplesSpin->setMinimum(1);
	samplesSpin->setMaximum(1000);
	samplesSpin->setValue(90);
	layout->addWidget(samplesSpin);

	auto* smoothButton = new QPushButton("Smooth");
	layout->addWidget(smoothButton);

	auto smoothChange = [this, smoothSlider, samplesSpin]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			alg->smooth(Number<Inexact>(smoothSlider->value() / (double)smoothSlider->maximum()), samplesSpin->value());
			updatePaintings();
		}
		};
	connect(smoothSlider, &QSlider::valueChanged, smoothChange);
	connect(samplesSpin, &QSpinBox::textChanged, smoothChange);
	connect(smoothButton, &QPushButton::clicked, smoothChange);

	auto* clearButton = new QPushButton("Clear Smooth");
	layout->addWidget(clearButton);

	connect(clearButton, &QPushButton::clicked, [this]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasSmoothResult()) {
			alg->clearSmoothResult();
			updatePaintings();
		}
		});
}

void SimplificationGUI::addOutputTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Output"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Output</h3>"));

	auto* saveShpbutton = new QPushButton("Save SHP file");
	layout->addWidget(saveShpbutton);

	connect(saveShpbutton, &QPushButton::clicked, [this]() {
		if (m_regions == nullptr) {
			std::cout << "Cannot save Shp file, no regions were loaded." << std::endl;
			return;
		}

		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (!alg->hasResult()) {
			std::cout << "Cannot save Shp file, current algorithm has no result." << std::endl;
			return;
		}

		std::filesystem::path filePath = QFileDialog::getSaveFileName(this, tr("Select Shapefile"), curr_dir, tr("Shapefile (*.shp)")).toStdString();
		if (filePath == "") return;
		curr_dir = QString::fromStdU16String(filePath.parent_path().u16string());

		InputGraph* graph = alg->resultToGraph();

		exportRegionSetUsingGDAL<InputGraph>(filePath, graph, *m_regions, m_spatialRef);

		//std::filesystem::path filePath = QFileDialog::getExistingDirectory(this, tr("Select folder for Shp file"), curr_dir).toStdString();
		//if (filePath == "") return;
		//curr_dir = QString::fromStdU16String(filePath.u16string());

		//InputGraph* graph = alg->resultToGraph();
		//
		//exportRegionSetUsingGDAL<InputGraph>(filePath, graph, *m_regions, m_spatialRef);

		});
}

void SimplificationGUI::addSettingsTab() {
	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Settings"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("Render these vertices:"));
	vertexMode = new QComboBox();
	vertexMode->addItem("Only degree-0 vertices");
	vertexMode->addItem("No degree-2 vertices");
	vertexMode->addItem("All vertices");
	vertexMode->setCurrentIndex(0);
	layout->addWidget(vertexMode);

	connect(vertexMode, static_cast<void (QComboBox::*)(int)>(&QComboBox::activated), [this](int index) {
		updatePaintings();
		});

	layout->addWidget(new QLabel("Search-tree depths"));
	depthSpin = new QSpinBox();
	depthSpin->setMinimum(1);
	depthSpin->setMaximum(20);
	depthSpin->setValue(10);
	layout->addWidget(depthSpin);
}

SimplificationGUI::SimplificationGUI() {
	setWindowTitle("Simplification");

	algorithms.push_back(&VWSimplifier::getInstance());
	algorithms.push_back(&KSBBSimplifier::getInstance());

	auto* dockWidget = new QDockWidget();
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
	tabs = new QTabWidget();
	dockWidget->setWidget(tabs);

	addInputTab();
	addPreprocessTab();
	addSimplifyTab();
	addPostprocessTab();
	addOutputTab();
	addSettingsTab();

	m_renderer = new GeometryWidget();
	m_renderer->setDrawAxes(false);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.00001);
	m_renderer->setMaxZoom(10000.0);

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
	if (m_regions != nullptr) {
		delete m_regions;
		delete m_spatialRef;
	}
}

void SimplificationGUI::loadInput(const std::filesystem::path& path, const int depth) {
	if (input != nullptr) {
		delete input;

		for (SimplificationAlgorithm* alg : algorithms) {
			alg->clear();
		}
	}

	if (m_regions != nullptr) {
		delete m_regions;
		m_regions = nullptr;
		delete m_spatialRef;
		m_spatialRef = nullptr;
	}

	if (path.extension() == ".ipe") {
		input = readIpeFile<InputGraph>(path, depth);
	}
	else if (path.extension() == ".shp") {
		auto [rs, sr] = readRegionSetUsingGDAL(path);
		m_regions = rs;
		m_spatialRef = sr;
		input = constructGraphAndRegisterBoundaries(*m_regions, depth);
	}
	else {
		std::cout << "Unexpected file extension: " << path.extension() << std::endl;
	}

	updatePaintings();

	if (input != nullptr) {
		input->orient();
		desiredComplexity->setMaximum(input->getEdgeCount());
		complexitySlider->setMaximum(input->getEdgeCount());
		m_renderer->fitInView(utils::boxOf<InputGraph::Vertex, MyKernel>(input->getVertices()).bbox());
	}
}
