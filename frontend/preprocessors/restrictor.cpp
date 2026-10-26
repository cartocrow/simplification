#include "restrictor.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressDialog>
#include <QLineEdit>

#include "frontend/widgets/orientation_widget.h"
#include "frontend/widgets/progress_indicator.h"
#include "library/orientation_restriction.h"

using namespace cartocrow::simplification;

RegularRestriction::RegularRestriction(PersistentSettings& settings) : Preprocessor("Regular Angle Restriction", settings) {
	m_count = m_settings.getInteger("regularrestriction_count", 2);
	m_angle = m_settings.getInteger("regularrestriction_angle", 0);
}

QWidget* RegularRestriction::constructWidget() {

	auto* result = new QWidget();
	auto* layout = new QVBoxLayout(result);
	layout->setAlignment(Qt::AlignTop);

	auto* orientSpin = new QSpinBox();
	auto* initAngleSpin = new QSpinBox();
	auto* preview = new OrientationWidget();

	auto updatePreview = [preview, this]() {
		m_settings.setInteger("regularrestriction_count", m_count);
		m_settings.setInteger("regularrestriction_angle", m_angle);
		preview->setOrientationsDegrees(m_count, m_angle);
		};
	updatePreview();

	{
		QHBoxLayout* row = new QHBoxLayout();
		row->addWidget(new QLabel("Number of orientations:"));
		orientSpin->setMinimum(2);
		orientSpin->setMaximum(20);
		orientSpin->setValue(m_count);
		row->addWidget(orientSpin);
		result->connect(orientSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, updatePreview](int count) {
			m_count = count;
			updatePreview();
			});
		layout->addLayout(row);
	}

	{
		initAngleSpin->setMinimum(0);
		initAngleSpin->setMaximum(180);
		initAngleSpin->setValue(m_angle);
		result->connect(initAngleSpin, QOverload<int>::of(&QSpinBox::valueChanged), [this, updatePreview](int angle) {
			m_angle = angle;
			updatePreview();
			});

		QHBoxLayout* row = new QHBoxLayout();
		row->addWidget(new QLabel("First angle (degrees):"));
		row->addWidget(initAngleSpin);
		layout->addLayout(row);
	}

	{
		auto* btnZero = new QPushButton("0");
		result->connect(btnZero, &QPushButton::clicked, [initAngleSpin, updatePreview]() {
			initAngleSpin->setValue(0);
			});
		auto* btnHalf = new QPushButton("50%");
		result->connect(btnHalf, &QPushButton::clicked, [initAngleSpin, orientSpin, updatePreview]() {
			int a = static_cast<int>(std::round(90.0 / orientSpin->value()));
			initAngleSpin->setValue(a);
			});

		QHBoxLayout* row = new QHBoxLayout();
		row->addWidget(new QLabel(""));
		row->addWidget(btnZero);
		row->addWidget(btnHalf);
		layout->addLayout(row);
	}

	layout->addWidget(preview);

	return result;
}

void RegularRestriction::applyTo(std::shared_ptr<InputGraph> graph) {
	ProgressIndicator progress("Restricting orientations", false);
	restrict_orientations(*graph, m_count, m_angle, 1, 0.1, [&progress](std::string label, int c, int m) {
		progress.smart_update(label, c, m);
		});
	progress.close();
}

IrregularRestriction::IrregularRestriction(PersistentSettings& settings) : Preprocessor("Irregular Angle Restriction", settings) {
	m_string = m_settings.getString("irregularrestriction_angles", "0 45 90");
}

QWidget* IrregularRestriction::constructWidget() {

	auto* result = new QWidget();
	auto* layout = new QVBoxLayout(result);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("Space-separated angles (degrees), at least two:"));

	QLineEdit* textField = new QLineEdit();
	textField->setText(QString::fromStdString(m_string));
	layout->addWidget(textField);

	auto* preview = new OrientationWidget();
	preview->setOrientationsRadians(extractAngles());
	layout->addWidget(preview);

	result->connect(textField, &QLineEdit::textChanged, [this, preview](QString text) {
		m_string = text.toStdString();
		m_settings.getString("irregularrestriction_angles", m_string);
		preview->setOrientationsRadians(extractAngles());
		});

	return result;
}

void IrregularRestriction::applyTo(std::shared_ptr<InputGraph> graph) {
	ProgressIndicator progress("Restricting orientations", false);

	std::vector<Number<Inexact>> angles = extractAngles();
	if (angles.size() < 2) {
		std::cout << "Enter at least two, distinct orientations" << std::endl;
		return;
	}

	restrict_orientations(*graph, angles, 1, 0.1, [&progress](std::string label, int c, int m) {
		progress.smart_update(label, c, m);
		});
	progress.close();
}

std::vector<Number<Inexact>> IrregularRestriction::extractAngles() {
	std::stringstream ss(m_string);
	std::vector<std::string> string_angles(
		(std::istream_iterator<std::string>(ss)),
		std::istream_iterator<std::string>()
	);
	std::vector<Number<Inexact>> angles;
	for (const auto& s : string_angles) {
		try {
			angles.push_back(std::stod(s) * std::numbers::pi / 180.0);
		}
		catch (const std::invalid_argument& e) {
			std::cout << "Warning: unparsable angle " << s << " is omitted" << std::endl;
		}
	}
	std::sort(angles.begin(), angles.end());
	return angles;
}