#include "smoother.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSlider>

#include <cartocrow/data_structures/graph_map_2.h>

#include "frontend/widgets/progress_indicator.h"

Smoother::Smoother(PersistentSettings& settings) : Postprocessor("Smoother", settings) {
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

	using Vertex_handle = OutputGraph::Vertex_handle;
	using Edge_handle = OutputGraph::Edge_handle;
	using Pt = Point<OutputGraph::Kernel>;
	using Vec = Vector<OutputGraph::Kernel>;
	using Num = Number<OutputGraph::Kernel>;

	size_t vtx_cnt = graph->number_of_vertices();

	Graph_static_vertex_map<OutputGraph, Num> rads(*graph, 0.0);

	ProgressIndicator progress("Smoothing", false);

	// determine smoothing radii
	Num max_rad = 0;
	progress.start_phase("Determining radii", vtx_cnt);
	for (size_t i = 0; i < vtx_cnt; ++i) {
		progress.update(i);

		Vertex_handle v = graph->vertex(i);

		if (v->degree() != 2) continue;

		Edge_handle inc = v->incoming();
		Edge_handle out = v->outgoing();

		Num in_len = std::sqrt(inc->curve().squared_length());
		Num out_len = std::sqrt(out->curve().squared_length());

		Num v_rad = std::min(in_len, out_len) / 2.0;
		if (v_rad > max_rad) max_rad = v_rad;

		rads[v] = v_rad;
	}

	// impose max
	max_rad *= (m_radiuspct / 100.0);

	// apply smoothing
	progress.start_phase("Applying smoothing", vtx_cnt);
	for (int i = 0; i < vtx_cnt; i++) {
			progress.update(i);

		Vertex_handle v = graph->vertex(i);
		if (v->degree() != 2) continue;

		Num v_rad = std::min(rads[v], max_rad);

		Edge_handle inc = v->incoming();
		Edge_handle out = v->outgoing();

		Num in_len = std::sqrt(inc->curve().squared_length());
		Num out_len = std::sqrt(out->curve().squared_length());

		Pt pt_v = v->point();

		Vec inc_vec = v->prev()->point() - pt_v;
		inc_vec /= in_len;
		Pt start = pt_v + v_rad * inc_vec;

		Vec out_vec = v->next()->point() - pt_v;
		out_vec /= out_len;
		Pt end = pt_v + v_rad * out_vec;

		auto is = CGAL::intersection(Line<Inexact>(start, inc_vec.perpendicular(CGAL::COUNTERCLOCKWISE)), Line<Inexact>(end, out_vec.perpendicular(CGAL::COUNTERCLOCKWISE)));
		if (!is.has_value()) {
			// collinear
			continue;
		}
		Pt* ctr = std::get_if<Point<Inexact>>(&*is);
		if (ctr == nullptr) {
			// collinear
			continue;
		}

		bool ccw = CGAL::right_turn(v->prev()->point(), pt_v, v->next()->point());

		// move to start of arc
		graph->move_vertex(v, start);

		// create end of arc
		Edge_handle edge = graph->subdivide_edge(out, end)->incoming();

		// introduce intermediate samples

		Vec arm = start - *ctr;
		Vec endarm = end - *ctr;

		// NB: angle is always less than 180 degrees by construction
		Num dotp = CGAL::scalar_product(arm, endarm) / std::sqrt(arm.squared_length() * endarm.squared_length());
		if (dotp > 1) {
			dotp = 1;
		}
		Num angle = std::acos(dotp);

		int samples = (int)std::floor(m_samples * angle / std::numbers::pi);

		if (ccw) {
			// arc is clockwise
			angle *= -1;
		}

		CGAL::Aff_transformation_2<Inexact> rot(CGAL::ROTATION, std::sin(angle / samples), std::cos(angle / samples));

		while (samples > 1) {
			arm = arm.transform(rot);
			Pt pt = *ctr + arm;
			edge = graph->subdivide_edge(edge, pt)->outgoing();
			samples--;
		}
	}

	// erase zero-length edges (if two adjacent vertices are both constrained by their shared edge)
	{
		int init_edge_count = graph->number_of_edges();
		int i = 0;
		int ii = 0;
		// invariant: all edges with index < i have sufficient length, except for indices in "revisit"
		std::vector<int> revisit;

		progress.start_phase("Cleaning up geometry", init_edge_count);
		while (i < graph->number_of_edges()) {
			progress.update(ii++);

			int next_i;
			if (revisit.empty()) {
				next_i = i;
			}
			else {
				next_i = revisit.back();
				revisit.pop_back();
			}

			Edge_handle e = graph->edge(next_i);

			if (e->curve().squared_length() <= 0.00001) {
				// remove unless between degree-3 vertices
				if (e->source()->degree() == 2) {
					graph->merge_vertex(e->source());
					// this removes e itself
				}
				else if (e->target()->degree() == 2) {
					Edge_handle nxt = e->next();
					if (nxt->graph_index() < i) {
						revisit.push_back(nxt->graph_index());
					}
					graph->merge_vertex(e->target());
					// this removes the next edge					
					if (next_i == i)
						i++;
				}
				else {
					// cannot remove, just go to next edge
					if (next_i == i)
						i++;
				}
			}
			else {
				// keep it
				if (next_i == i)
					i++;
			}
		}
	}

}