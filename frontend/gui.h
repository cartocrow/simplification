#pragma once

#include <QMainWindow>
#include <QApplication>

#include <cartocrow/core/core.h>
#include <cartocrow/reader/ipe_reader.h>
#include <cartocrow/renderer/geometry_painting.h>
#include <cartocrow/renderer/geometry_widget.h>

#include "library/straight_graph.h"
#include "library/vertex_removal.h"

#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;
using namespace cartocrow::simplification;

void launchGUI(int argc, char* argv[]);

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer = nullptr;
	InputGraph* input = nullptr;
	std::vector<SimplificationAlgorithm*> algorithms;

	void updatePaintings();
public:
	SimplificationGUI();
	~SimplificationGUI();

	void loadInput(const std::filesystem::path& path);
};


