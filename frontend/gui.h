#pragma once

#include <QMainWindow>
#include <QApplication>
#include <QSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QLabel>

#include <ogrsf_frmts.h>

#include <cartocrow/renderer/geometry_widget.h>

#include "region_set.h"
#include "simplification_algorithm.h"

using namespace cartocrow;
using namespace cartocrow::renderer;

void launchGUI(int argc, char* argv[]);

class SimplificationGUI : public QMainWindow {
	//Q_OBJECT

private:
	GeometryWidget* m_renderer = nullptr;
	RegionSet<Exact>* m_regions = nullptr;
	std::optional<std::string> m_spatialRef;
	InputGraph* input = nullptr;
	InputGraph* preprocessed = nullptr;
	std::vector<SimplificationAlgorithm*> algorithms;

	QTabWidget* tabs;
	// IO
	QString curr_dir = ".";
	QLabel* curr_file;
	QLabel* curr_srs;
	// preprocess
	
	// simplify
	QComboBox* algorithmSelector;
	QSpinBox* desiredComplexity;
	QSlider* complexitySlider;
	// postprocess

	// settings
	QComboBox* vertexMode;
	QSpinBox* depthSpin;
	Color m_input_color = Color{ 60,60,60};
	Color m_preprocessed_color = Color{ 20,20,20 };

	void addIOTab();
	void addPreprocessTab();
	void addSimplifyTab();
	void addPostprocessTab();
	void addSettingsTab();

	void updatePaintings();
public:
	SimplificationGUI();
	~SimplificationGUI();

	void loadInput(InputGraph* graph, const bool keepregions);
	void loadInput(const std::filesystem::path& path, const int depth);
};


