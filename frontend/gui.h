#pragma once

#include <QMainWindow>
#include <QApplication>
#include <QSpinBox>
#include <QComboBox>
#include <QSlider>

#include <cartocrow/renderer/geometry_widget.h>

#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

void launchGUI(int argc, char* argv[]);

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer = nullptr;
	InputGraph* input = nullptr;
	std::vector<SimplificationAlgorithm*> algorithms;

	QSpinBox* desiredComplexity;
	QComboBox* vertexMode;
	QSlider* complexitySlider;

	void updatePaintings();
public:
	SimplificationGUI();
	~SimplificationGUI();

	void loadInput(const std::filesystem::path& path, const int depth);
};


