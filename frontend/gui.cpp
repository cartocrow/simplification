#include "gui.h"

void launchGUI(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SimplificationGUI gui;
	gui.show();
	app.exec();
}

SimplificationGUI::SimplificationGUI() {
	setWindowTitle("Isoline simplification");

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
	m_renderer->setDrawAxes(true);
	setCentralWidget(m_renderer);

	m_renderer->setMinZoom(0.01);
	m_renderer->setMaxZoom(1000.0);
}