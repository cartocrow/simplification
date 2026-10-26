#include "orientation_widget.h"

#include <numbers>
#include <QColor>
#include <QPainter>

OrientationWidget::OrientationWidget(QWidget* parent) : QWidget(parent) {
	setMinimumSize(100, 100);
}

void OrientationWidget::setOrientationsDegrees(const int& count, const double& init_degrees) {
	setOrientationsRadians(count, degToRad(init_degrees));
}

void OrientationWidget::setOrientationsRadians(const int& count, const double& init_radians) {
	m_angles.resize(count);
	const double step = std::numbers::pi / count;
	for (int i = 0; i < count; i++) {
		m_angles[i] = init_radians + i * step;
	}
	update();
}

void OrientationWidget::setOrientationsDegrees(const std::vector<double>& degrees) {
	m_angles.resize(degrees.size());
	for (int i = 0; i < degrees.size(); i++) {
		m_angles[i] = degToRad(degrees[i]);
	}
	std::sort(m_angles.begin(), m_angles.end());
	update();
}

void OrientationWidget::setOrientationsRadians(const std::vector<double>& radians) {
	m_angles.resize(radians.size());
	for (int i = 0; i < radians.size(); i++) {
		m_angles[i] = radians[i];
	}
	std::sort(m_angles.begin(), m_angles.end());
	update();
}

std::vector<double> OrientationWidget::getOrientationsRadians() {
	return m_angles;
}
std::vector<double> OrientationWidget::getOrientationsDegrees() {
	std::vector<double> degrees;
	for (const double& d : m_angles) {
		degrees.push_back(radToDeg(d));
	}
	return degrees;
}

QSize OrientationWidget::sizeHint() const {
	return QSize(200, 200);
}
QSize OrientationWidget::minimumSizeHint() const { 
	return QSize(100, 100); 
}

double OrientationWidget::radToDeg(const double& rad) {
	constexpr double conv = 180.0 / std::numbers::pi;
	return rad * conv;
}
double OrientationWidget::degToRad(const double& deg) {
	constexpr double conv = std::numbers::pi / 180.0;
	return deg * conv;
}

void OrientationWidget::paintEvent(QPaintEvent* event) {

	constexpr QColor bg_color = QColor(240, 240, 240);
	
	constexpr double angle_width = 2;
	constexpr QColor angle_color = QColor(30, 30, 30);

	constexpr double ring_width = 1;
	constexpr QColor ring_color = QColor(150, 150, 150);
	constexpr int tick_each = 5; // degrees
	constexpr int major_each = 3; // num ticks
	constexpr double ring_frac = 0.85;
	constexpr double major_frac_in = 0.85; 
	constexpr double major_frac_out = 0.95;
	constexpr double minor_frac_in = 0.85;
	constexpr double minor_frac_out = 0.90;
	constexpr int num_ticks = 360 / tick_each;

	QRect box = rect();
	box.adjust(-1, -1, 1, 1);

	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setRenderHint(QPainter::TextAntialiasing, true);
	p.fillRect(box, bg_color);

	const QPointF ctr = QRectF(box).center();
	const double radius = std::min(box.width(), box.height()) * 0.5;

	p.setBrush(Qt::NoBrush);
	p.setPen(QPen(ring_color, ring_width));
	p.drawEllipse(ctr, radius * ring_frac, radius * ring_frac);

	for (int k = 0; k < num_ticks; ++k) {
		const double angle = degToRad(k * tick_each);
		const QPointF dir(std::cos(angle), -std::sin(angle)); // y inverted
		const double frac_in = (k % major_each == 0) ? major_frac_in : minor_frac_in;
		const double frac_out = (k % major_each == 0) ? major_frac_out : minor_frac_out;
		p.drawLine(ctr + dir * radius * frac_in, ctr + dir * radius * frac_out);
	}

	p.setPen(QPen(angle_color, angle_width));
	for (const double& angle : m_angles) {
		const QPointF dir(std::cos(angle), -std::sin(angle)); // y inverted
		p.drawLine(ctr - dir * radius * ring_frac, ctr + dir * radius * ring_frac);
	}
}