#include "gui.h"

#include <QCheckBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

#include "library/utils.h"

#include "vw.h"
#include "ksbb.h"
#include "ksbb_inexact.h"
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
		auto paint = std::make_shared<GraphPainting<InputGraph>>(*input, m_input_color, 1, vmode);
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

void SimplificationGUI::addIOTab() {
	auto* tab = new QWidget();
	tabs->addTab(tab, tr("I/O"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Input</h3>"));

	auto* loadFileButton = new QPushButton("Load file");
	layout->addWidget(loadFileButton);

	auto* txt = new QLabel("<p>Currently supported file formats:</p><ul><li>Shapefiles (*.shp) containing (multi)polygons with holes.</li><li>Geojson files (*.geojson) containing (multi)polygons with holes.</li><li>IPE files (*.ipe) containing polygons, polylines and points. Note that these cannot be saved to a Shapefile currently.</li></ul>");
	txt->setWordWrap(true);
	layout->addWidget(txt);

	layout->addWidget(new QLabel("<h3>Currently loaded</h3>"));

	curr_file = new QLabel("<i>No file</i>");
	curr_file->setWordWrap(true);
	layout->addWidget(curr_file);
	curr_srs = new QLabel("<i>No spatial reference</i>");
	curr_srs->setWordWrap(true);
	layout->addWidget(curr_srs);


	connect(loadFileButton, &QPushButton::clicked, [this]() {
		std::filesystem::path filePath = QFileDialog::getOpenFileName(this, tr("Select map"), curr_dir, tr("Accepted formats (*.shp *.geojson *.ipe);;Shapefiles (*.shp);;Geojson (*.geojson);;IPE files(*.ipe)")).toStdString();
		if (filePath == "") return;
		curr_dir = QString::fromStdU16String(filePath.parent_path().u16string());

		QProgressDialog progress("Loading file", nullptr, 0, 2, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(1);

		loadInput(filePath, this->depthSpin->value());

		progress.setValue(2);
		});

	layout->addWidget(new QLabel("<h3>Output</h3>"));

	auto* txt2 = new QLabel("<p>This only works if a Shp/Geojson file was loaded. It saves the result of the currently selected algorithm in the Simplify tab; its postprocessed variant, if available, and its unprocessed result otherwise.</p>");
	txt2->setWordWrap(true);
	layout->addWidget(txt2);

	auto* saveShpbutton = new QPushButton("Save SHP file");
	layout->addWidget(saveShpbutton);

	connect(saveShpbutton, &QPushButton::clicked, [this]() {
		if (m_regions == nullptr) {
			std::cout << "Cannot save Shp file, no regions were loaded." << std::endl;
			return;
		}

		std::filesystem::path filePath = QFileDialog::getSaveFileName(this, tr("Select Shapefile"), curr_dir, tr("Shapefile (*.shp)")).toStdString();
		if (filePath == "") return;
		curr_dir = QString::fromStdU16String(filePath.parent_path().u16string());


		QProgressDialog progress("Exporting", nullptr, 0, 2, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(1);

		InputGraph* graph;
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (!alg->hasResult()) {
			std::cout << "Warning: selected algorithm has no result -- exporting input graph instead." << std::endl;
			graph = input;
		}
		else {
			graph = alg->resultToGraph();
		}

		exportRegionSetUsingGDAL<InputGraph>(filePath, graph, *m_regions, m_spatialRef);

		progress.setValue(2);

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

	auto runAlg = [this](SimplificationAlgorithm* alg, int target) {

		int start = alg->getComplexity();

		if (target == start) {
			return;
		}

		QProgressDialog progress("Simplifying", "Stop", 0, target < start ? start - target : target - start, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(0);

		alg->runToComplexity(target,
			[&progress, &target, &start](int c) {
				std::string lbl = "Complexity " + std::to_string(c);
				progress.setLabelText(QString::fromStdString(lbl));
				progress.setValue(target < start ? start - c : target - c);
			},
			[&progress]() {
				return progress.wasCanceled();
			});

		progress.setValue(target < start ? start - target : target - start);

		desiredComplexity->setValue(alg->getComplexity());
		complexitySlider->setValue(alg->getComplexity());
		m_renderer->repaint();
		};

	connect(initButton, &QPushButton::clicked, [this]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (input != nullptr) {

			QProgressDialog progress("Initializing", nullptr, 0, 2, this);
			progress.setWindowModality(Qt::WindowModal);
			progress.setMinimumDuration(1000);
			progress.setValue(1);

			alg->initialize(input, depthSpin->value());

			progress.setValue(2);

			desiredComplexity->setValue(alg->getComplexity());
			complexitySlider->setValue(alg->getComplexity());
			updatePaintings();
		}
		});

	connect(reverseButton, &QPushButton::clicked, [this, stepSpin, runAlg]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, alg->getComplexity() + stepSpin->value());
		}
		});

	connect(stepButton, &QPushButton::clicked, [this, stepSpin, runAlg]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, alg->getComplexity() - stepSpin->value());
		}
		});

	connect(runButton, &QPushButton::clicked, [this, runAlg]() {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, desiredComplexity->value());
		}
		});

	connect(stepSpin, &QSpinBox::textChanged, [this, stepSpin]() {
		complexitySlider->setSingleStep(stepSpin->value());
		});

	connect(complexitySlider, &QSlider::valueChanged, [this, runAlg](int value) {
		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, value);
		}
		});
}

void SimplificationGUI::addPostprocessTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Postprocess"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Postprocess</h3>"));

	layout->addWidget(new QLabel("Smooth radius (% of max)"));
	auto* smoothSpin = new QSpinBox();
	smoothSpin->setMinimum(1);
	smoothSpin->setMaximum(100);
	smoothSpin->setValue(50);
	layout->addWidget(smoothSpin);
	auto* smoothSlider = new QSlider();
	smoothSlider->setFocusPolicy(Qt::StrongFocus);
	smoothSlider->setTickPosition(QSlider::TicksBothSides);
	smoothSlider->setTickInterval(10);
	smoothSlider->setSingleStep(1);
	smoothSlider->setOrientation(Qt::Horizontal);
	smoothSlider->setMinimum(1);
	smoothSlider->setMaximum(100);
	smoothSlider->setValue(50);
	layout->addWidget(smoothSlider);


	layout->addWidget(new QLabel("Edges on semicircle"));
	auto* samplesSpin = new QSpinBox();
	samplesSpin->setMinimum(1);
	samplesSpin->setMaximum(1000);
	samplesSpin->setValue(45);
	layout->addWidget(samplesSpin);

	auto* smoothButton = new QPushButton("Smooth");
	layout->addWidget(smoothButton);

	auto smoothChange = [this, smoothSpin, smoothSlider, samplesSpin]() {

		SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {

			int c = alg->getComplexity();

			QProgressDialog progress("Smoothing", nullptr, 0, c, this);
			progress.setWindowModality(Qt::WindowModal);
			progress.setMinimumDuration(1000);
			progress.setValue(0);

			alg->smooth(Number<Inexact>(smoothSlider->value() / (double)smoothSlider->maximum()), samplesSpin->value(),
				[&progress](std::string phase, int index, int max) {
					if (index % 100 == 0) { // dont perform all updates to gui...
						progress.setLabelText(QString::fromStdString(phase));
						progress.setMaximum(max);
						progress.setValue(index);
					}

				});

			progress.setValue(progress.maximum());

			updatePaintings();
		}
		};
	connect(smoothSpin, &QSpinBox::textChanged, [this, smoothSpin, smoothSlider, smoothChange] {
		smoothSlider->setValue(smoothSpin->value());
		smoothChange(); });
	connect(smoothSlider, &QSlider::valueChanged, [this, smoothSpin, smoothSlider, smoothChange] {
		smoothSpin->setValue(smoothSlider->value());
		smoothChange(); });
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
	algorithms.push_back(&KSBBInexactSimplifier::getInstance());

	auto* dockWidget = new QDockWidget();
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
	tabs = new QTabWidget();
	dockWidget->setWidget(tabs);

	addIOTab();
	//addPreprocessTab();
	addSimplifyTab();
	addPostprocessTab();
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
	}
}

void SimplificationGUI::loadInput(const std::filesystem::path& path, const int depth) {
	if (input != nullptr) {
		delete input;

		for (SimplificationAlgorithm* alg : algorithms) {
			alg->clear();
		}
		input = nullptr;
	}

	if (m_regions != nullptr) {
		delete m_regions;
		m_regions = nullptr;
		m_spatialRef = std::nullopt;
	}

	if (path.extension() == ".ipe") {
		input = readIpeFile<InputGraph>(path, depth);
		curr_file->setText(QString::fromStdString(path.filename().string()));
		curr_srs->setText(QString::fromStdString("<i>No spatial reference</i>"));
	}
	else if (path.extension() == ".shp" || path.extension() == ".geojson") {
		auto [rs, sr] = readRegionSetUsingGDAL(path);
		m_regions = rs;
		m_spatialRef = sr;
		curr_file->setText(QString::fromStdString(path.filename().string()));
		if (sr.has_value()) {
			if (sr.value().size() > 40) {
				curr_srs->setText(QString::fromStdString(sr.value().substr(0, 37) + "..."));
			}
			else {
				curr_srs->setText(QString::fromStdString(sr.value()));

			}
			
		}
		else {
			curr_srs->setText(QString::fromStdString("<i>No spatial reference</i>"));
		}
		input = constructGraphAndRegisterBoundaries(*m_regions, depth);
	}
	else {
		std::cout << "Unexpected file extension: " << path.extension() << std::endl;
		curr_file->setText(QString::fromStdString("<i>No file</i>"));
		curr_srs->setText(QString::fromStdString("<i>No spatial reference</i>"));
	}

	updatePaintings();

	if (input != nullptr) {
		input->orient();
		desiredComplexity->setMaximum(input->getEdgeCount());
		complexitySlider->setMaximum(input->getEdgeCount());
		m_renderer->fitInView(utils::boxOf<InputGraph::Vertex, Exact>(input->getVertices()).bbox());
	}
}
