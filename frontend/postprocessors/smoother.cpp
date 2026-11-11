#include "smoother.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>

#include "library/smoothing.h"

#include "frontend/widgets/progress_indicator.h"

Smoother::Smoother(PersistentSettings& settings) : Postprocessor("Smooth", settings) {
	m_radiuspct = m_settings.getInteger("smoother_radiuspercent", 50);
	m_samples = m_settings.getInteger("smoother_samples", 45);
}

QWidget* Smoother::constructWidget(std::function<void()> autoapply) {

	auto* result = new QWidget();
	auto* layout = new QVBoxLayout(result);
	layout->setAlignment(Qt::AlignTop);

	layout->addWidget(new QLabel("Smooth radius (% of max)"));
	auto* smoothSpin = new QSpinBox();
	smoothSpin->setMinimum(1);
	smoothSpin->setMaximum(100);
	smoothSpin->setValue(m_radiuspct);
	layout->addWidget(smoothSpin);
	auto* smoothSlider = new QSlider();
	smoothSlider->setFocusPolicy(Qt::StrongFocus);
	smoothSlider->setTickPosition(QSlider::TicksBothSides);
	smoothSlider->setTickInterval(10);
	smoothSlider->setSingleStep(1);
	smoothSlider->setOrientation(Qt::Horizontal);
	smoothSlider->setMinimum(1);
	smoothSlider->setMaximum(100);
	smoothSlider->setValue(m_radiuspct);
	layout->addWidget(smoothSlider);

	layout->addWidget(new QLabel("Edges on semicircle"));
	auto* samplesSpin = new QSpinBox();
	samplesSpin->setMinimum(1);
	samplesSpin->setMaximum(1000);
	samplesSpin->setValue(m_samples);
	layout->addWidget(samplesSpin);

	result->connect(smoothSpin, &QSpinBox::textChanged, [this, smoothSpin, smoothSlider, autoapply] {
		m_radiuspct = smoothSpin->value();
		smoothSlider->setValue(m_radiuspct);
		m_settings.setInteger("smoother_radiuspercent", m_radiuspct);
		autoapply();
		});
	result->connect(smoothSlider, &QSlider::valueChanged, [this, smoothSpin, smoothSlider, autoapply] {
		m_radiuspct = smoothSlider->value();
		smoothSpin->setValue(m_radiuspct);
		m_settings.setInteger("smoother_radiuspercent", m_radiuspct);
		autoapply();
		});
	result->connect(samplesSpin, &QSpinBox::textChanged, [this, samplesSpin, autoapply]() {
		m_samples = samplesSpin->value();
		m_settings.setInteger("smoother_samples", m_samples);
		autoapply();
		});

	return result;

}

void Smoother::applyTo(std::shared_ptr<OutputGraph> graph) {
	ProgressIndicator progress("Smoothing", false);
	std::string currentPhase;
	simplification::smooth(*graph, m_radiuspct, m_samples, [&](std::string label, int current, int max) {
		if (label != currentPhase) {
			currentPhase = label;
			progress.start_phase(label, max);
		} else {
			progress.update(current);
		}
	});
}
