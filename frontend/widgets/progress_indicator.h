#pragma once

#include <QProgressDialog>

class ProgressIndicator {
private:
	QProgressDialog m_dialog;
	int m_update;

public:
	ProgressIndicator(const std::string& header, const bool allow_cancel);

	void start_phase(std::string label, int max);
	void update(int value);
	void update(std::string label, int value);

	void smart_update(std::string label, int value, int max);

	void close();

	bool wasCanceled();
};