#include "commandline.h"

#include <cartocrow/core/stopwatch.h>

#include "ipe_reader.h"
#include "library/vertex_removal.h"

using namespace cartocrow;
using namespace cartocrow::simplification;
using namespace std;

template<bool ExactMode> 
void runVW(const CommandLineArguments& cla) {

	cout << "Running VW (ExactMode = " << ExactMode << ")" << endl;

	using Kernel = std::conditional<ExactMode, Exact, Inexact>::type;
	using Graph = VertexRemovalGraph<Kernel, false>;
	using PQT = VertexQuadTree<Graph>;
	using Alg = VisvalingamWhyatt<Graph>;

	bool errored = false;
	if (!cla.has_argument("-input")) {
		cout << "Error: missing -input argument with input file location" << endl;
		errored = true;
	} 
	if (!cla.has_argument("-target")) {
		cout << "Error: missing -target argument with desired number of edges" << endl;
		errored = true;
	}
	if (!cla.has_argument("-output")) {
		cout << "Error: missing -output argument with output file location" << endl;
		errored = true;
	}
	if (errored) {
		return;
	}

	filesystem::path input = cla.get_argument("-input");
	int complexity = stoi(cla.get_argument("-target"));
	filesystem::path output = cla.get_argument("-output");

	StopwatchPool pool("Timers");

	Stopwatch& load = pool.get("load");
	Stopwatch& init = pool.get("init");
	Stopwatch& run = pool.get("run");

	Graph graph;
	cout << "Loading " << input << endl;
	load.start();
	auto res = readIpeFile<Graph>(graph, input, 10, 0.00001);
	if (!res) {
		return;
	}
	PQT pqt = *res;
	graph.initialize();
	load.stop();
	cout << "  Done, " << graph.number_of_edges() << " edges" << endl;

	Alg alg(graph, pqt);
	cout << "Initializing" << endl;
	init.start();
	alg.initialize(false);
	init.stop();
	cout << "Running" << endl;
	run.start();
	alg.run([](int complexity, Number<Kernel> cost) {
		return complexity <= 1; });
	run.stop();
	cout << "Done, " << graph.number_of_edges() << " edges" << endl;

	pool.printAll();
}

void runCommand(const CommandLineArguments& cla) {

	string alg = cla.get_argument("-alg");
	bool exact = cla.has_argument("-exact");
	if (alg == "VW") {
		if (exact) {
			runVW<true>(cla);
		}
		else {
			runVW<false>(cla);
		}
	}
}