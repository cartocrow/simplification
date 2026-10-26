#include "gui.h"

#include <QDockWidget>
#include <QFileDialog>
#include <QProgressDialog>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QFrame>
#include <QLineEdit>
#include <QStackedWidget>

#include <cartocrow/renderer/graph_painting.h>
#include <cartocrow/utils/stopwatch.h>

#include "library/utils.h"

#include "widgets/orientation_widget.h"

#include "ipe_reader.h"
#include "read_graph_gdal.h"

#include "preprocessors/restrictor.h"

#include "simplifiers/vw.h"
#include "simplifiers/pvw.h"
#include "simplifiers/ksbb.h"
#include "simplifiers/bmrs.h"

#include "postprocessors/smoother.h"

using namespace cartocrow;
using namespace cartocrow::simplification;

void launchGUI(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SimplificationGUI gui;
	gui.show();
	app.exec();
}

void SimplificationGUI::updatePaintings() {
	m_renderer->clear();

	VertexSelection vmode = static_cast<VertexSelection>(vertexMode->currentIndex());

	if (input != nullptr) {
		auto paint = std::make_shared<GraphPainting<InputGraph>>(input, VertexStyle{ m_input_color }, EdgeStyle{ m_input_color, 1 }, vmode);
		m_renderer->addPainting(paint, "Input");
	}

	if (preprocessed != nullptr) {
		auto paint = std::make_shared<GraphPainting<InputGraph>>(preprocessed, VertexStyle{ m_preprocessed_color }, EdgeStyle{ m_preprocessed_color, 2 }, vmode);
		m_renderer->addPainting(paint, "Preprocessed");
	}

	for (Simplifier* alg : algorithms) {
		if (alg->hasResult()) {
			m_renderer->addPainting(alg->getPainting(vmode), alg->name());
		}
		if (alg->hasPostprocessed()) {
			m_renderer->addPainting(alg->getPostprocessedPainting(), alg->name() + " " + alg->postprocessedName());
		}
	}

	m_renderer->update();
}

void SimplificationGUI::addIOTab() {
	auto* tab = new QWidget();
	tabs->addTab(tab, tr("IO"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	curr_dir = QString::fromStdString(m_settings.getString("dir", "."));

	layout->addWidget(new QLabel("<h3>Input</h3>"));

	auto* loadFileButton = new QPushButton("Load file");
	layout->addWidget(loadFileButton);

	auto* txt = new QLabel("<p>Currently supported file formats:</p><ul><li>Shapefiles (*.shp) containing (multi)polygons with holes.</li><li>Geojson files (*.geojson) containing (multi)polygons with holes.</li><li>GeoPackage files (*.gpkg) containing (multi)polygons with holes.</li><li>IPE files (*.ipe) containing polygons, polylines and points. Note that these cannot be saved to a Shapefile currently.</li></ul>");
	txt->setWordWrap(true);
	layout->addWidget(txt);


	auto* outToInButton = new QPushButton("Set current result to input");
	layout->addWidget(outToInButton);

	connect(loadFileButton, &QPushButton::clicked, [this]() {
		std::filesystem::path filePath = QFileDialog::getOpenFileName(this, tr("Select map"), curr_dir,
			tr("Accepted formats (*.shp *.geojson *.ipe *.gpkg);;Shapefiles (*.shp);;Geojson (*.geojson);;GeoPackage (*.gpkg);;IPE files (*.ipe)")).toStdString();
		if (filePath == "") return;
		curr_dir = QString::fromStdU16String(filePath.parent_path().u16string());
		m_settings.setString("dir", curr_dir.toStdString());

		QProgressDialog progress("Loading file", nullptr, 0, 2, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(1);

		loadInput(filePath, this->depthSpin->value());

		progress.setValue(2);
		});



	connect(outToInButton, &QPushButton::clicked, [this]() {

		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (!alg->hasResult()) {
			std::cout << "No result available for current algorithm." << std::endl;
			return;
		}

		QProgressDialog progress("Converting", nullptr, 0, 2, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(1);

		loadInput(alg->resultToInputGraph(), true);

		progress.setValue(2);
		});

	layout->addWidget(new QLabel("<h3>Currently loaded</h3>"));

	curr_file = new QLabel("<i>No file</i>");
	curr_file->setWordWrap(true);
	layout->addWidget(curr_file);
	curr_srs = new QLabel("<i>No spatial reference</i>");
	curr_srs->setWordWrap(true);
	layout->addWidget(curr_srs);

	layout->addWidget(new QLabel("<h3>Output</h3>"));

	auto* txt2 = new QLabel("<p>This only works if a shp/geojson/gpkg file was loaded. It saves the result of the currently selected algorithm in the Simplify tab; its postprocessed variant, if available, and its unprocessed result otherwise.</p>");
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
		m_settings.setString("dir", curr_dir.toStdString());

		QProgressDialog progress("Exporting", nullptr, 0, 2, this);
		progress.setWindowModality(Qt::WindowModal);
		progress.setMinimumDuration(1000);
		progress.setValue(1);

		std::shared_ptr<InputGraph> graph;
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (!alg->hasResult()) {
			std::cout << "Warning: selected algorithm has no result -- exporting input / preprocessed graph instead." << std::endl;
			if (!preprocessed) {
				graph = input;
			}
			else {
				graph = preprocessed;
			}
		}
		else {
			graph = alg->resultToInputGraph();
		}

		exportRegionSetUsingGDAL<InputGraph>(filePath, graph.get(), *m_regions, m_spatialRef);

		progress.setValue(2);

		});
}

void SimplificationGUI::addPreprocessTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Prep"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Preprocess</h3>"));

	auto* prepSelector = new QComboBox();
	layout->addWidget(prepSelector);

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	layout->addWidget(new QLabel("<b>Settings</b>"));
	auto* stack = new QStackedWidget();
	layout->addWidget(stack, 0, Qt::AlignTop);

	for (Preprocessor* prep : preprocessors) {
		prepSelector->addItem(QString::fromStdString(prep->name()));
		stack->addWidget(prep->constructWidget());
	}
	int curr = m_settings.getInteger("prep_selection", 0);
	stack->setCurrentIndex(curr);
	prepSelector->setCurrentIndex(curr);

	connect(prepSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [stack, this](int index) {
		stack->setCurrentIndex(index);
		m_settings.setInteger("prep_selection", index);
		});

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	auto* buttonApply = new QPushButton("Apply");
	layout->addWidget(buttonApply);
	connect(buttonApply, &QPushButton::clicked, [this, prepSelector]() {
		Preprocessor* prep = preprocessors[prepSelector->currentIndex()];
		preprocessed = std::make_shared<InputGraph>();
		graph_2_copy(*input, *preprocessed);
		prep->applyTo(preprocessed);
		updatePaintings();
		});

	auto* buttonClear = new QPushButton("Clear preprocessed");
	layout->addWidget(buttonClear);
	connect(buttonClear, &QPushButton::clicked, [this]() {
		preprocessed = nullptr;
		updatePaintings();
		});
}

void SimplificationGUI::addSimplifyTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Simplify"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Simplify</h3>"));

	int default_alg = m_settings.getInteger("selected_algorithm", 0);
	algorithmSelector = new QComboBox();
	layout->addWidget(algorithmSelector);

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	layout->addWidget(new QLabel("<b>Settings</b>"));
	auto* stack = new QStackedWidget();
	layout->addWidget(stack, 0, Qt::AlignTop);

	for (Simplifier* alg : algorithms) {
		algorithmSelector->addItem(QString::fromStdString(alg->name()));
		stack->addWidget(alg->constructWidget());
	}
	algorithmSelector->setCurrentIndex(default_alg);
	stack->setCurrentIndex(default_alg);

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	auto* initButton = new QPushButton("Initialize");
	layout->addWidget(initButton);

	auto* initAndRunButton = new QPushButton("Initialize and run fully");
	layout->addWidget(initAndRunButton);

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	int default_step = m_settings.getInteger("step_size", 1);
	auto* stepSpinLabel = new QLabel("Step size");
	layout->addWidget(stepSpinLabel);
	auto* stepSpin = new QSpinBox();
	stepSpin->setMinimum(1);
	stepSpin->setMaximum(1000);
	stepSpin->setValue(default_step);
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

	auto runAlg = [this](Simplifier* alg, int target) {

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
				if (c % 100 == 0) {
					std::string lbl = "Complexity " + std::to_string(c);
					progress.setLabelText(QString::fromStdString(lbl));
					progress.setValue(target < start ? start - c : target - c);
				}
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
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (input != nullptr) {

			QProgressDialog progress("Initializing", nullptr, 0, 2, this);
			progress.setWindowModality(Qt::WindowModal);
			progress.setMinimumDuration(1000);
			progress.setValue(1);

			if (preprocessed == nullptr) {
				alg->initialize(input, depthSpin->value());
			}
			else {
				alg->initialize(preprocessed, depthSpin->value());
			}

			progress.setValue(2);

			int c = alg->getComplexity();
			desiredComplexity->setMaximum(c);
			desiredComplexity->setValue(c);
			complexitySlider->setMaximum(c);
			complexitySlider->setValue(c);
			updatePaintings();
		}
		});

	connect(initAndRunButton, &QPushButton::clicked, [this, runAlg]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		cartocrow::utils::StopwatchPool pool("Init & run");
		if (input != nullptr) {

			QProgressDialog progress("Initializing", nullptr, 0, 2, this);
			progress.setWindowModality(Qt::WindowModal);
			progress.setMinimumDuration(1000);
			progress.setValue(1);

			auto& timer = pool.get("init");
			timer.start();
			if (preprocessed == nullptr) {
				alg->initialize(input, depthSpin->value());
			}
			else {
				alg->initialize(preprocessed, depthSpin->value());
			}
			timer.stop();

			progress.setValue(2);

			int c = alg->getComplexity();
			desiredComplexity->setMaximum(c);
			desiredComplexity->setValue(c);
			complexitySlider->setMaximum(c);
			complexitySlider->setValue(c);


			auto& timer2 = pool.get("run");
			timer2.start();

			if (alg->hasResult()) {
				runAlg(alg, 1);
			}
			timer2.stop();

			pool.printAndClear();

			updatePaintings();
		}
		});

	connect(reverseButton, &QPushButton::clicked, [this, stepSpin, runAlg]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, alg->getComplexity() + stepSpin->value());
		}
		});

	connect(stepButton, &QPushButton::clicked, [this, stepSpin, runAlg]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, alg->getComplexity() - stepSpin->value());
		}
		});

	connect(runButton, &QPushButton::clicked, [this, runAlg]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, desiredComplexity->value());
		}
		});

	connect(stepSpin, &QSpinBox::textChanged, [this, stepSpin]() {
		complexitySlider->setSingleStep(stepSpin->value());
		m_settings.setInteger("step_size", stepSpin->value());
		});

	connect(complexitySlider, &QSlider::valueChanged, [this, runAlg](int value) {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			runAlg(alg, value);
		}
		});

	connect(algorithmSelector, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this, stack](int value) {

		Simplifier* alg = algorithms[value];
		m_settings.setInteger("selected_algorithm", value);
		int mc, c;
		if (alg->hasResult()) {
			mc = alg->getMaximumComplexity();
			c = alg->getComplexity();
		}
		else {
			mc = c = 1;
		}
		desiredComplexity->setMaximum(mc);
		desiredComplexity->setValue(c);
		complexitySlider->setMaximum(mc);
		complexitySlider->setValue(c);

		stack->setCurrentIndex(value);
		});

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	auto* clearButton = new QPushButton("Clear result");
	layout->addWidget(clearButton);
	connect(clearButton, &QPushButton::clicked, [this]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		alg->clear();
		updatePaintings();
		});

	//auto* debugButton = new QPushButton("Draw current state");
	//layout->addWidget(debugButton);
	//connect(debugButton, &QPushButton::clicked, [this]() {
	//	SimplificationAlgorithm* alg = algorithms[algorithmSelector->currentIndex()];
	//	for (std::pair<std::shared_ptr<GeometryPainting>,std::string> p : alg->getDebugPaintings()) {
	//		m_renderer->addPainting(p.first, p.second);
	//	}
	//	});
}

void SimplificationGUI::addPostprocessTab() {

	auto* tab = new QWidget();
	tabs->addTab(tab, tr("Post"));
	auto* layout = new QVBoxLayout(tab);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("<h3>Postprocess</h3>"));

	auto* postSelector = new QComboBox();
	layout->addWidget(postSelector);

	auto apply = [this, postSelector]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasResult()) {
			Postprocessor* post = postprocessors[postSelector->currentIndex()];
			alg->applyPostprocessor(post);
			updatePaintings();
		}
		};

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	layout->addWidget(new QLabel("<b>Settings</b>"));
	auto* stack = new QStackedWidget();
	layout->addWidget(stack, 0, Qt::AlignTop);

	for (Postprocessor* post : postprocessors) {
		postSelector->addItem(QString::fromStdString(post->name()));
		stack->addWidget(post->constructWidget(apply));
	}
	int curr = m_settings.getInteger("post_selection", 0);
	stack->setCurrentIndex(curr);
	postSelector->setCurrentIndex(curr);

	connect(postSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), [stack, this](int index) {
		stack->setCurrentIndex(index);
		m_settings.setInteger("post_selection", index);
		});

	{
		QFrame* line = new QFrame(this);
		line->setFrameShape(QFrame::HLine);
		layout->addWidget(line);
	}

	auto* buttonApply = new QPushButton("Apply");
	layout->addWidget(buttonApply);
	connect(buttonApply, &QPushButton::clicked, apply);

	auto* clearButton = new QPushButton("Clear");
	layout->addWidget(clearButton);
	connect(clearButton, &QPushButton::clicked, [this]() {
		Simplifier* alg = algorithms[algorithmSelector->currentIndex()];
		if (alg->hasPostprocessed()) {
			alg->clearPostprocessed();
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

	algorithms.push_back(new VWSimplifier(m_settings));
	algorithms.push_back(new PVWSimplifier(m_settings));
	algorithms.push_back(new KSBBSimplifier(m_settings));
	algorithms.push_back(new BMRSSimplifier(m_settings));

	preprocessors.push_back(new RegularRestriction(m_settings));
	preprocessors.push_back(new IrregularRestriction(m_settings));

	postprocessors.push_back(new Smoother(m_settings));

	auto* dockWidget = new QDockWidget();
	addDockWidget(Qt::LeftDockWidgetArea, dockWidget);
	tabs = new QTabWidget();
	dockWidget->setWidget(tabs);

	addIOTab();
	addPreprocessTab();
	addSimplifyTab();
	addPostprocessTab();
	addSettingsTab();

	m_renderer = new GeometryWidget();
	m_renderer->setDrawAxes(false);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.0000001);
	m_renderer->setMaxZoom(1000000.0);

	updatePaintings();
}

SimplificationGUI::~SimplificationGUI() {
	for (Preprocessor* prep : preprocessors) {
		delete prep;
	}
	for (Simplifier* alg : algorithms) {
		alg->clear();
		delete alg;
	}
	if (m_renderer != nullptr) {
		delete m_renderer;
	}
	input = nullptr;
	preprocessed = nullptr;

	if (m_regions != nullptr) {
		delete m_regions;
	}
}

void SimplificationGUI::loadInput(std::shared_ptr<InputGraph> graph, const bool keepregions) {
	if (input != nullptr) {
		for (Simplifier* alg : algorithms) {
			alg->clear();
		}
		input = nullptr;
	}

	if (preprocessed != nullptr) {
		preprocessed = nullptr;
	}

	if (!keepregions) {
		if (m_regions != nullptr) {
			delete m_regions;
			m_regions = nullptr;
			m_spatialRef = std::nullopt;
		}
	}

	input = graph;

	updatePaintings();

	if (input) {
		input->initialize();
		desiredComplexity->setMaximum(input->number_of_edges());
		complexitySlider->setMaximum(input->number_of_edges());
		m_renderer->fitInView(input->bbox());
	}
}

void SimplificationGUI::loadInput(const std::filesystem::path& path, const int depth) {

	if (m_regions != nullptr) {
		delete m_regions;
		m_regions = nullptr;
		m_spatialRef = std::nullopt;
	}

	std::shared_ptr<InputGraph> graph;
	if (path.extension() == ".ipe") {
		graph.reset(readIpeFile<InputGraph>(path, depth, M_EPSILON));
		curr_file->setText(QString::fromStdString(path.filename().string()));
		curr_srs->setText(QString::fromStdString("<i>No spatial reference</i>"));
	}
	else if (path.extension() == ".shp" || path.extension() == ".geojson" || path.extension() == ".gpkg") {
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
		graph.reset(constructGraphAndRegisterBoundaries(*m_regions, depth));
	}
	else {
		std::cout << "Unexpected file extension: " << path.extension() << std::endl;
		curr_file->setText(QString::fromStdString("<i>No file</i>"));
		curr_srs->setText(QString::fromStdString("<i>No spatial reference</i>"));
		graph = nullptr;
	}

	loadInput(graph, true);
}
