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

#include "library/straight_graph.h"
#include "library/vertex_removal.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

void launchGUI(int argc, char* argv[]);

using MyKernel = Inexact;
using InputGraph = StraightGraph<VoidData, VoidData, MyKernel>;
using OutputGraph = VertexRemovalGraph<MyKernel>;
using OutputPQT = PointQuadTree<OutputGraph::Vertex, MyKernel>;

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer = nullptr;
	InputGraph* input = nullptr;
	OutputGraph* output = nullptr;
	OutputPQT* outpqt = nullptr;
	VisvalingamWhyatt<OutputGraph>* vw = nullptr;

	void repaint();
	void initialize();
public:
	SimplificationGUI();
	~SimplificationGUI();

};


