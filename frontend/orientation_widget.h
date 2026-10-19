#pragma once

#include <QWidget>

class OrientationWidget : public QWidget {
	Q_OBJECT

public:
	explicit OrientationWidget(QWidget* parent = nullptr);

	void setOrientationsDegrees(const int& count, const double& init_degrees);
	void setOrientationsRadians(const int& count, const double& init_radians);
	void setOrientationsDegrees(const std::vector<double>& degrees);
	void setOrientationsRadians(const std::vector<double>& radians);

	std::vector<double> getOrientationsRadians();
	std::vector<double> getOrientationsDegrees();

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	double radToDeg(const double& rad);
	double degToRad(const double& deg);

	std::vector<double> m_angles; // in radians

};