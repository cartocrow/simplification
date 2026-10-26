#include "progress_indicator.h"

ProgressIndicator::ProgressIndicator(const std::string& header, const bool allow_cancel) : m_dialog(QString::fromStdString(header), "Stop", 0, 100) {
	if (!allow_cancel) {
		m_dialog.setWindowFlags(m_dialog.windowFlags() & ~Qt::WindowCloseButtonHint);
	}
	m_dialog.setWindowModality(Qt::WindowModal);
	m_dialog.setMinimumDuration(1000);
	m_dialog.setValue(0);
	m_update = 1;
}

void ProgressIndicator::start_phase(std::string label, int max) {
	m_dialog.setLabelText(QString::fromStdString(label));
	m_dialog.setValue(0);
	m_dialog.setMaximum(max);
	m_update = std::max(1, max / 100);
}


void ProgressIndicator::update(int value) {
	if (value % m_update == 0) {
		m_dialog.setValue(value);
	}
}

void ProgressIndicator::update(std::string label, int value) {
	m_dialog.setLabelText(QString::fromStdString(label));	
	m_dialog.setValue(value);
}

void ProgressIndicator::smart_update(std::string label, int value, int max) {
	if (!label.empty()) {
		m_dialog.setLabelText(QString::fromStdString(label));
	}
	if (max > 0) {
		m_dialog.setValue(0);
		m_dialog.setMaximum(max);
		m_dialog.setValue(value);
		m_update = std::max(1, max / 100);
	}
	else {
		m_dialog.setValue(value);
	}
}

void ProgressIndicator::close() {
	m_dialog.setValue(m_dialog.maximum());
}

bool ProgressIndicator::wasCanceled() {
	return m_dialog.wasCanceled();
}