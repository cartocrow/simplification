#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "frontend/plain_graphs.h"
#include "frontend/region_set.h"
#include "library/edge_collapse.h"
#include "library/edge_moves.h"
#include "library/orientation_restriction.h"
#include "library/path_vertex_removal.h"
#include "library/smoothing.h"
#include "library/vertex_removal.h"

using namespace emscripten;

using namespace cartocrow;
using namespace cartocrow::simplification;

/// Call f, and turn C++ exceptions into JavaScript errors with their message.
/// With -fwasm-exceptions, a C++ exception reaches JavaScript as a WebAssembly.Exception, which has no readable
/// message and cannot be sent from a web worker to the main thread.
template <typename F> auto withJSErrors(F f) -> decltype(f()) {
	try {
		return f();
	} catch (const std::exception& e) {
		val::global("Error").new_(std::string(e.what())).throw_();
	} catch (...) {
		val::global("Error").new_(std::string("Unknown C++ exception")).throw_();
	}
}

/// The search-tree depth, as the default in the Qt GUI.
constexpr int depth = 10;

/// The coordinates of the paths of a graph, as an array with a Float64Array [x0, y0, x1, y1, ...] per path, in the
/// order of the path indices. The regions refer to the paths by these indices; see Simplification::loadRegions.
template <class Graph> val pathCoordinates(Graph& graph) {
	val result = val::array();
	std::vector<double> coordinates;
	for (size_t i = 0; i < graph.number_of_paths(); ++i) {
		auto path = graph.path(i);
		coordinates.clear();
		auto add = [&](const auto& point) {
			coordinates.push_back(CGAL::to_double(point.x()));
			coordinates.push_back(CGAL::to_double(point.y()));
		};
		auto e = path->first_edge();
		add(e->source()->point());
		while (true) {
			add(e->target()->point());
			if (e == path->last_edge()) break;
			e = e->next();
		}
		result.call<void>("push", val::global("Float64Array").new_(typed_memory_view(coordinates.size(), coordinates.data())));
	}
	return result;
}

/// A simplification algorithm with its own copy of the graph, which keeps its history, so that going back to a higher
/// complexity undoes steps instead of recomputing them.
struct AlgorithmState {
	virtual ~AlgorithmState() = default;
	/// Simplify to complexity k, or undo steps until the complexity is k. progress is called with the complexity while
	/// the algorithm runs.
	virtual void runToComplexity(int k, std::optional<std::function<void(int)>> progress) = 0;
	virtual int complexity() = 0;
	virtual val paths() = 0;
	virtual std::shared_ptr<OutputGraph> toOutputGraph() = 0;
};

template <class DT, class Alg> struct State : AlgorithmState {
	using Graph = typename DT::Graph;
	Graph m_graph;
	std::unique_ptr<Alg> m_alg;

	/// create makes the algorithm for the graph and initializes it.
	template <class Create> State(InputGraph& input, Create create) {
		graph_2_copy(input, m_graph);
		m_alg = create(m_graph);
	}

	void runToComplexity(int k, std::optional<std::function<void(int)>> progress) override {
		// As runToComplexity of the simplifiers of the Qt GUI.
		auto& hist = m_graph.history();
		if (k > (int)m_graph.number_of_edges()) {
			while (hist.can_undo() && (int)m_graph.number_of_edges() < k) {
				hist.undo();
			}
		} else if (k < (int)m_graph.number_of_edges()) {
			while (hist.can_redo() && (int)m_graph.number_of_edges() > k) {
				hist.redo();
			}
			if (!hist.can_redo() && k < (int)m_graph.number_of_edges()) {
				m_alg->run([&](int complexity, auto) {
					if (progress) (*progress)(complexity);
					return complexity <= k;
				});
			}
		}
	}

	int complexity() override {
		return m_graph.number_of_edges();
	}

	val paths() override {
		return pathCoordinates(m_graph);
	}

	std::shared_ptr<OutputGraph> toOutputGraph() override {
		auto result = std::make_shared<OutputGraph>();
		graph_2_copy(m_graph, *result);
		return result;
	}
};

/// The state of algorithm Alg on a copy of graph. create makes the algorithm for the copy and initializes it.
template <class DT, class Alg, class Create> std::unique_ptr<AlgorithmState> makeState(InputGraph& graph, Create create) {
	return std::make_unique<State<DT, Alg>>(graph, create);
}

/// The algorithm for the method and settings, as the simplifiers of the Qt GUI choose it.
std::unique_ptr<AlgorithmState> makeAlgorithm(InputGraph& graph, const std::string& method, val settings) {
	if (method == "vw") {
		auto make = [&]<class K>() {
			using DT = vr::DataTraits_Dedicated<K, true>;
			using Alg = vr::VertexRemoval<DT, vr::EffectiveAreaScore<DT>>;
			return makeState<DT, Alg>(graph, [](typename DT::Graph& g) {
				auto alg = std::make_unique<Alg>(g);
				alg->initialize(depth);
				return alg;
			});
		};
		return settings["exact"].as<bool>() ? make.template operator()<Exact>() : make.template operator()<Inexact>();
	}
	if (method == "pvw") {
		// The weight is a fraction; the Qt GUI asks for it as a percentage.
		double weight = settings["weight"].as<double>();
		auto make = [&]<class K>() {
			using DT = pvr::DataTraits_Dedicated<K, true, Number<K>>;
			using Score = pvr::BiasedEffectiveAreaScore<DT>;
			using Alg = pvr::PathVertexRemoval<DT, Score>;
			return makeState<DT, Alg>(graph, [weight](typename DT::Graph& g) {
				auto alg = std::make_unique<Alg>(g, Score(weight));
				alg->initialize(depth);
				return alg;
			});
		};
		return settings["exact"].as<bool>() ? make.template operator()<Exact>() : make.template operator()<Inexact>();
	}
	if (method == "ksbb") {
		auto make = [&]<class K, bool ApproxExact>() {
			using DT = ec::DataTraits_Dedicated<K, true>;
			using Alg = ec::EdgeCollapse<DT, ec::MinimumSymmetricDifferenceCollapse<DT, ApproxExact>>;
			return makeState<DT, Alg>(graph, [](typename DT::Graph& g) {
				auto alg = std::make_unique<Alg>(g);
				alg->initialize(depth, 0.05);
				return alg;
			});
		};
		// 0: inexact, 1: semi-exact, 2: exact.
		int exactness = settings["exactness"].as<int>();
		if (exactness == 1) return make.template operator()<Exact, true>();
		if (exactness == 2) return make.template operator()<Exact, false>();
		return make.template operator()<Inexact, false>();
	}
	if (method == "bmrs") {
		auto make = [&]<class K, bool AllowFiniteNoSupport>() {
			using DT = em::DataTraits_Dedicated<K, true>;
			using Alg = em::EdgeMoves<DT, em::SweptAreaScore<DT, AllowFiniteNoSupport>>;
			return makeState<DT, Alg>(graph, [](typename DT::Graph& g) {
				auto alg = std::make_unique<Alg>(g);
				alg->initialize(depth, 0.05);
				return alg;
			});
		};
		bool exact = settings["exact"].as<bool>();
		bool allow = settings["allowFiniteNoSupport"].as<bool>();
		if (exact) return allow ? make.template operator()<Exact, true>() : make.template operator()<Exact, false>();
		return allow ? make.template operator()<Inexact, true>() : make.template operator()<Inexact, false>();
	}
	throw std::runtime_error("Unknown simplification method " + method);
}

/// The simplification pipeline for use from JavaScript: the regions are loaded once into a graph, optionally with
/// restricted orientations, then simplified, then optionally smoothed. All inputs and outputs are plain JavaScript
/// values; the geometry is returned as the coordinates of the paths of the graph (see pathCoordinates).
class Simplification {
  private:
	std::shared_ptr<InputGraph> m_input;
	/// The input with restricted orientations, if they are restricted.
	std::shared_ptr<InputGraph> m_restricted;
	std::unique_ptr<AlgorithmState> m_algorithm;

  public:
	/// Load the regions: an array with per region an array of polygons, each an array of rings, each a Float64Array
	/// [x0, y0, x1, y1, ...] without repeating the first point. Returns {edges, regions}, where regions has per
	/// region an array of rings, each an array of arcs [path index, reversed]: the ring follows these paths in order.
	/// progress is called with the number of processed points and the total; see constructGraphAndRegisterBoundaries.
	val loadRegions(val regions, val progress) {
		return withJSErrors([&] {
			RegionSet<Exact> regionSet;
			const int regionCount = regions["length"].as<int>();
			for (int r = 0; r < regionCount; ++r) {
				Region<Exact> region;
				val polygons = regions[r];
				const int polygonCount = polygons["length"].as<int>();
				for (int p = 0; p < polygonCount; ++p) {
					val rings = polygons[p];
					const int ringCount = rings["length"].as<int>();
					for (int i = 0; i < ringCount; ++i) {
						std::vector<double> coordinates = convertJSArrayToNumberVector<double>(rings[i]);
						Polygon<Exact> ring;
						for (size_t j = 0; j + 1 < coordinates.size(); j += 2) {
							ring.push_back(Point<Exact>(coordinates[j], coordinates[j + 1]));
						}
						region.rings.push_back(ring);
					}
					region.ringcounts.push_back(ringCount);
				}
				regionSet.push_back(region);
			}

			m_input.reset(constructGraphAndRegisterBoundaries(regionSet, depth, [&progress](int processed, int total) {
				if (!progress.isUndefined()) progress(processed, total);
			}));
			m_restricted = nullptr;
			m_algorithm = nullptr;

			val regionArcs = val::array();
			for (const auto& region : regionSet) {
				val rings = val::array();
				for (const auto& registration : region.arcs) {
					val arcs = val::array();
					for (const auto& arc : registration) {
						val a = val::array();
						a.call<void>("push", arc.boundary);
						a.call<void>("push", arc.reverse);
						arcs.call<void>("push", a);
					}
					rings.call<void>("push", arcs);
				}
				regionArcs.call<void>("push", rings);
			}

			val result = val::object();
			result.set("edges", (int)m_input->number_of_edges());
			result.set("regions", regionArcs);
			return result;
		});
	}

	/// The paths of the input.
	val inputPaths() {
		return withJSErrors([&] { return pathCoordinates(*m_input); });
	}

	/// Restrict the orientations of the edges of the input to the given angles in degrees, by replacing edges by
	/// staircases. Returns {edges, paths}. progress is called with the label of a phase, its progress and its maximum.
	val restrictOrientations(val angles, val progress) {
		return withJSErrors([&] {
			std::vector<double> degrees = convertJSArrayToNumberVector<double>(angles);
			std::vector<Number<Inexact>> radians;
			for (double d : degrees) radians.push_back(d * std::numbers::pi / 180);
			std::sort(radians.begin(), radians.end());
			m_restricted = std::make_shared<InputGraph>();
			graph_2_copy(*m_input, *m_restricted);
			restrict_orientations(*m_restricted, radians, 1, 0.1, [&progress](std::string label, int current, int max) {
				if (!progress.isUndefined()) progress(label, current, max);
			});
			m_algorithm = nullptr;
			val result = val::object();
			result.set("edges", (int)m_restricted->number_of_edges());
			result.set("paths", pathCoordinates(*m_restricted));
			return result;
		});
	}

	/// Stop using restricted orientations.
	void clearRestriction() {
		m_restricted = nullptr;
		m_algorithm = nullptr;
	}

	/// Prepare the method ("vw", "pvw", "ksbb" or "bmrs") with its settings, and simplify as far as possible, so that
	/// any complexity can be shown by undoing steps. Uses the restricted input if there is one. progress is called
	/// with the complexity. Returns the minimum complexity.
	int prepare(const std::string& method, val settings, val progress) {
		return withJSErrors([&] {
			m_algorithm = makeAlgorithm(m_restricted ? *m_restricted : *m_input, method, settings);
			m_algorithm->runToComplexity(0, [&progress](int complexity) {
				if (!progress.isUndefined()) progress(complexity);
			});
			return m_algorithm->complexity();
		});
	}

	/// Show the prepared simplification at complexity k (the number of edges). Returns {edges, paths}.
	val setComplexity(int k) {
		return withJSErrors([&] {
			m_algorithm->runToComplexity(k, std::nullopt);
			val result = val::object();
			result.set("edges", m_algorithm->complexity());
			result.set("paths", m_algorithm->paths());
			return result;
		});
	}

	/// Smooth the simplification at its current complexity. radiusPercent is the largest radius as a percentage of
	/// the largest possible radius, and samples the number of edges of a semicircle. Returns {edges, paths}.
	val smooth(int radiusPercent, int samples) {
		return withJSErrors([&] {
			auto graph = m_algorithm->toOutputGraph();
			simplification::smooth(*graph, radiusPercent, samples);
			val result = val::object();
			result.set("edges", (int)graph->number_of_edges());
			result.set("paths", pathCoordinates(*graph));
			return result;
		});
	}
};

EMSCRIPTEN_BINDINGS(simplification) {
	class_<Simplification>("Simplification")
	    .constructor<>()
	    .function("loadRegions", &Simplification::loadRegions)
	    .function("inputPaths", &Simplification::inputPaths)
	    .function("restrictOrientations", &Simplification::restrictOrientations)
	    .function("clearRestriction", &Simplification::clearRestriction)
	    .function("prepare", &Simplification::prepare)
	    .function("setComplexity", &Simplification::setComplexity)
	    .function("smooth", &Simplification::smooth);
}
