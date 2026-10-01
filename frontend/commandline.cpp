#include "commandline.h"

#include <numbers>
#include <cartocrow/utils/stopwatch.h>

#include "ipe_reader.h"
#include "library/vertex_removal.h"
#include "library/edge_collapse.h"
#include "library/orientation_restriction.h"
#include "library/edge_moves.h"

using namespace cartocrow;
using namespace cartocrow::utils;
using namespace cartocrow::simplification;
using namespace std;

template<bool ExactMode>
void runVW(const CommandLineArguments& cla) {

	cout << "Running VW (ExactMode = " << ExactMode << ")" << endl;

	using Kernel = std::conditional<ExactMode, Exact, Inexact>::type;
	using Graph = VertexRemovalGraph<Kernel, false>;
	using PQT = VertexQuadTree<Graph>;
	using Alg = VisvalingamWhyatt<Graph>;

	filesystem::path input = cla.get_string("-input");
	int target = cla.get_integer("-target");
	filesystem::path output = cla.get_string("-output");
	int depth = cla.get_integer("-depth", 10);

	StopwatchPool pool("Timers");

	Stopwatch& load = pool.get("load");
	Stopwatch& init = pool.get("init");
	Stopwatch& run = pool.get("run");

	Graph graph;
	cout << "Loading " << input << endl;
	load.start();
	PQT* pqt = readIpeFile<Graph>(graph, input, depth, 0.0000001);

	if (pqt == nullptr) {
		return;
	}
	graph.initialize();

	load.stop();
	cout << "  Done, " << graph.number_of_edges() << " edges" << endl;

	Alg alg(graph, *pqt);
	cout << "Initializing" << endl;
	init.start();
	alg.initialize(false);
	init.stop();
	run.start();
	alg.run([&target](int complexity, Number<Kernel> cost) {
		return complexity <= target; });
	run.stop();
	run.start();
	cout << "Done, " << graph.number_of_edges() << " edges" << endl;
	run.stop();

	delete pqt;

	writeIpeFile(graph, output);

	pool.printAll();
}

template<bool ExactMode>
void runKSBB(const CommandLineArguments& cla) {

	cout << "Running KSBB (ExactMode = " << ExactMode << ")" << endl;

	using Kernel = std::conditional<ExactMode, Exact, Inexact>::type;
	using Graph = EdgeCollapseGraph<Kernel, false>;
	using PQT = VertexQuadTree<Graph>;
	using SQT = EdgeQuadTree<Graph>;
	using Alg = KronenfeldEtAl<Graph>;

	filesystem::path input = cla.get_string("-input");
	int target = cla.get_integer("-target", 1);
	filesystem::path output = cla.get_string("-output");
	int depth = cla.get_integer("-depth", 10);
	Number<Kernel> fuzz = cla.get_double("-fuzzy", 0.05);

	StopwatchPool pool("Timers");

	Stopwatch& load = pool.get("load");
	Stopwatch& init = pool.get("init");
	Stopwatch& run = pool.get("run");

	Graph graph;
	cout << "Loading " << input << endl;
	load.start();
	auto res = readIpeFile<Graph>(graph, input, depth, 0.0000001);
	if (!res) {
		return;
	}
	PQT pqt = *res;
	graph.initialize();
	load.stop();
	cout << "  Done, " << graph.number_of_edges() << " edges" << endl;

	Rectangle box = pqt.root_box();
	SQT sqt(box, depth, fuzz);
	Alg alg(graph, sqt, pqt);
	cout << "Initializing" << endl;
	init.start();
	alg.initialize(false, true);
	init.stop();
	cout << "Running" << endl;
	run.start();
	alg.run([&target](int complexity, Number<Kernel> cost) {
		return complexity <= target; });
	run.stop();
	cout << "Done, " << graph.number_of_edges() << " edges" << endl;

	writeIpeFile(graph, output);

	pool.printAll();
}

template<bool ExactMode>
void runBMRS(const CommandLineArguments& cla) {

	cout << "Running BMRS (ExactMode = " << ExactMode << ")" << endl;

	using Kernel = std::conditional<ExactMode, Exact, Inexact>::type;
	using Graph = EdgeMovesGraph<Kernel, false>;
	using PQT = VertexQuadTree<Graph>;
	using SQT = EdgeQuadTree<Graph>;
	using Alg = BuchinEtAl<Graph>;

	filesystem::path input = cla.get_string("-input");
	int target = cla.get_integer("-target", 1);
	filesystem::path output = cla.get_string("-output");
	int depth = cla.get_integer("-depth", 10);
	Number<Kernel> fuzz = cla.get_double("-fuzzy", 0.05);

	StopwatchPool pool("Timers");

	Stopwatch& load = pool.get("load");
	Stopwatch& init = pool.get("init");
	Stopwatch& run = pool.get("run");

	Graph graph;
	cout << "Loading " << input << endl;
	load.start();
	auto res = readIpeFile<Graph>(graph, input, depth, 0.0000001);
	if (!res) {
		return;
	}
	PQT pqt = *res;
	graph.initialize();
	load.stop();
	cout << "  Done, " << graph.number_of_edges() << " edges" << endl;

	constexpr double deg_to_rad = numbers::pi / 180.0;

	if (cla.has_argument("-restrict", 2)) {
		int orientations = cla.get_integer("-restrict", 1);
		double angle = cla.get_double("-restrict", 2) * deg_to_rad;
		restrict_orientations(graph, orientations, angle);
	}

	Rectangle box = pqt.root_box();
	SQT sqt(box, depth, fuzz);
	Alg alg(graph, sqt, pqt);
	cout << "Initializing" << endl;
	init.start();
	alg.initialize(false, true);
	init.stop();
	cout << "Running" << endl;
	run.start();
	alg.run([&target](int complexity, Number<Kernel> cost) {
		return complexity <= target; });
	run.stop();
	cout << "Done, " << graph.number_of_edges() << " edges" << endl;

	writeIpeFile(graph, output);

	pool.printAll();
}

void runCommand(const CommandLineArguments& cla) {

	cla.print_arguments();
	cout << endl;

	bool errored = false;
	if (!cla.has_argument("-alg", 1)) {
		cout << "Error: missing -alg argument with desired algorithm (VW or KSBB)" << endl;
		errored = true;
	}
	if (!cla.has_argument("-input", 1)) {
		cout << "Error: missing -input argument with input file location" << endl;
		errored = true;
	}
	if (!cla.has_argument("-target", 1)) {
		cout << "Error: missing -target argument with desired number of edges" << endl;
		errored = true;
	}
	if (!cla.has_argument("-output", 1)) {
		cout << "Error: missing -output argument with output file location" << endl;
		errored = true;
	}
	if (errored) {
		return;
	}

	string alg = cla.get_string("-alg", "VW");
	bool exact = cla.has_argument("-exact");
	if (alg == "VW") {
		if (exact) {
			runVW<true>(cla);
		}
		else {
			runVW<false>(cla);
		}
	}
	else if (alg == "KSBB") {
		if (exact) {
			runKSBB<true>(cla);
		}
		else {
			runKSBB<false>(cla);
		}
	}
	else if (alg == "BMRS") {
		if (exact) {
			runBMRS<true>(cla);
		}
		else {
			runBMRS<false>(cla);
		}
	}
	else {
		cout << "Error: unexpected algorithm " << alg << endl;
		cout << "  should be one of VW, KSBB or BMRS" << endl;
	}
}