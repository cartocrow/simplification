#pragma once

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

void launchGUI(int argc, char* argv[]);

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer;

public:
	SimplificationGUI();
};


