#include <QMainWindow>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QMessageBox>

#include <cartocrow/core/core.h>
#include <cartocrow/reader/ipe_reader.h>
#include <cartocrow/renderer/geometry_painting.h>
#include <cartocrow/renderer/geometry_widget.h>

using namespace cartocrow;
using namespace cartocrow::renderer;

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer;

public:
	SimplificationGUI() {
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
};

int main(int argc, char* argv[]) {
	QApplication app(argc, argv);
	SimplificationGUI gui;
	gui.show();
	app.exec();
}