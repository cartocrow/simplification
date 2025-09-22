#pragma once

#include <cartocrow/core/core.h>

namespace cartocrow::simplification {

template <class G> struct Operation;

// edges store the last operation that has been performed on them

template <class Graph> struct Operation {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	Edge* pre_edge;
	Op* past;

	Operation(Edge* e) : pre_edge(e) {
		past = e->data().hist;
		e->data().hist = this;
	}

	virtual void undo(Graph& g) = 0;
	virtual void redo(Graph& g) = 0;
};

template <class Graph> struct SplitOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	SplitOperation(Edge* e, Point<typename Graph::Kernel> pt) : Operation<Graph>(e), post_loc(pt) {
		post_edge_1 = nullptr;
		post_edge_2 = nullptr;
		future_1 = nullptr;
		future_2 = nullptr;
	}

	// Result of the split
	Edge* post_edge_1;
	Edge* post_edge_2;
	Point<typename Graph::Kernel> post_loc;

	// edge histories of the result
	Op* future_1;
	Op* future_2;

	virtual void undo(Graph& g) {

		future_1 = post_edge_1->data().hist;
		future_2 = post_edge_2->data().hist;
			
		Vertex* v = post_edge_1->commonVertex(post_edge_2);

		Vertex* u;
		Vertex* w;
		if (v->edge(0)->getTarget() == v) {
			u = v->neighbor(0);
			w = v->neighbor(1);
		}
		else {
			u = v->neighbor(1);
			w = v->neighbor(0);
		}

		g.removeVertex(v);
		this->pre_edge = g.addEdge(u, w);

		if (this->past != nullptr) {
			this->pre_edge->data().hist = this->past;
		}
	}

	virtual void redo(Graph& g) {}
};

template <class Graph> struct EraseOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	EraseOperation(Vertex* v) : Operation<Graph>(v->edge(0)) {
		target = v == this->pre_edge->getTarget();
		pre_loc = v->getPoint();
		target_2 = v == v->edge(1)->getTarget();
		past_2 = v->edge(1)->data().hist;
	}

	bool target;
	bool target_2;
	Point<typename Graph::Kernel> pre_loc;

	// edge histories of the input
	Op* past_2 = nullptr;
	// edge histories of the result
	Op* future = nullptr;

	virtual void undo(Graph& g) {

		Vertex* u = this->pre_edge->getSource();
		Vertex* w = this->pre_edge->getTarget();

		g.removeEdge(this->pre_edge);

		Vertex* v = g.addVertex(pre_loc);
		if (target) {
			this->pre_edge = g.addEdge(u, v);
		}
		else {
			this->pre_edge = g.addEdge(v, u);
		}

		Edge* edge_2;
		if (target_2) {
			edge_2 = g.addEdge(w, v);
		}
		else {
			edge_2 = g.addEdge(v, w);
		}

		if (this->past != nullptr) {
			this->past->pre_edge = this->pre_edge;
		}

		if (past_2 != nullptr) {
			this->past->pre_edge = edge_2;
		}
	}

	virtual void redo(Graph& g) {

		Vertex* u = target ? this->pre_edge->getSource() : this->pre_edge->getTarget();
		Vertex* v = target ? this->pre_edge->getTarget() : this->pre_edge->getSource();
		Vertex* w = target ? this->pre_edge->targetWalkNeighbor() : this->pre_edge->sourceWalkNeighbor();

		g.removeVertex(v);
		this->pre_edge = g.addEdge(u, w);
	}
};

template <class Graph> struct ShiftOperation : public Operation<Graph> {

	using Edge = Graph::Edge;
	using Vertex = Graph::Vertex;
	using Op = Operation<Graph>;

	ShiftOperation(Vertex* v, Point<typename Graph::Kernel> pt) : Operation<Graph>(v.edge(0)) {
		target = v == this->pre_edge->getTarget();
		pre_loc = v->getPoint();
		post_loc = pt;
	}

	// Input to the split
	bool target;
	Point<typename Graph::Kernel> pre_loc;
	// Result of the split
	Point<typename Graph::Kernel> post_loc;

	virtual void undo(Graph& g) {

		Vertex* v = target ? this->pre_edge->target() : this->pre_edge->source();
		v->setPoint(pre_loc);

		if (this->past != nullptr) {
			this->past->pre_edge = this->pre_edge;
		}
	}

	virtual void redo(Graph& g) {

		Vertex* v = target ? this->pre_edge->target() : this->pre_edge->source();
		v->setPoint(post_loc);
	}
};

template <class Graph> struct OperationBatch {

	using Op = Operation<Graph>;

	~OperationBatch() {
		for (Op* op : operations) {
			delete op;
		}
	}

	std::vector<Op*> operations;

	/// Number of edges in the map after this operation
	int post_complexity;
	/// Maximum cost of operations up to this one
	Number<typename Graph::Kernel> post_maxcost;

	void undo(Graph& g) {
		// NB: reverse
		for (auto it = operations.rbegin(); it != operations.rend(); it++) {

			Op* op = *it;
			op->undo(g);
		}
	}
	void redo(Graph& g) {
		for (Op* op : operations) {
			op->redo(g);
		}
	}
};

template <class Graph> 
concept EdgeStoredOperations = requires(typename Graph::Edge::Data d) {
	
	{
		d.hist
	} -> std::same_as<Operation<Graph>*&>; // c++ shenanigans: the expression is still a handle, even if it's declared as a nonhandle.
};

template <class Graph> 
	requires EdgeStoredOperations<Graph>
class HistoricGraph {
  public:
	using Vertex = Graph::Vertex;
	using Edge = Graph::Edge;
	using Kernel = Graph::Kernel;
	using BaseGraph = Graph;

  private:
	using Batch = OperationBatch<Graph>;

	Graph& graph;

	Number<Kernel> max_cost;
	int in_complexity;
	Batch* building_batch = nullptr;

	std::vector<Batch*> history;
	std::vector<Batch*> undone;

  public:
	HistoricGraph(Graph& graph) : graph(graph), max_cost(0) {
		in_complexity = graph.getEdgeCount();
	}

	~HistoricGraph() {
		for (Batch* op : history) {
			delete op;
		}

		for (Batch* op : undone) {
			delete op;
		}
	}

	Graph& getBaseGraph() {
		return graph;
	}

	std::vector<Vertex*>& getVertices() {
		return graph.getVertices();
	}

	std::vector<Edge*>& getEdges() {
		return graph.getEdges();
	}

	int getEdgeCount() {
		return graph.getEdgeCount();
	}

	Edge* mergeVertex(Vertex* v) {

		assert(v->degree() == 2);
		assert(building_batch != nullptr);

		EraseOperation<Graph>* op = new EraseOperation<Graph>(v);
		op->redo(graph);
		building_batch->operations.push_back(op);

		return op->pre_edge;
	}

	Vertex* splitEdge(Edge* e, Point<Kernel> p) {

		SplitOperation<Graph>* op = new SplitOperation<Graph>(e, p);
		op->redo(graph);
		building_batch->operations.push_back(op);

		return op->post_edge_1->commonVertex(op->post_edge_2);
	}

	void shiftVertex(Vertex* v, Point<Kernel> p) {

		assert(v->degree() > 0);
		assert(building_batch != nullptr);

		ShiftOperation<Graph>* op = new ShiftOperation<Graph>(v, p);
		op->redo(graph);
		building_batch->operations.push_back(op);
	}

	void recallComplexity(int c) {

		assert(building_batch == nullptr);

		while ( // if history is a single element, check input complexity
		    (history.size() >= 1 && in_complexity <= c)
		    // if history is more than a single element, check the previous operation
		    || (history.size() > 1 && history[history.size() - 2]->post_complexity <= c)) {
			backInTime();
		}
		while (!undone.empty() && graph.getEdgeCount() > c) {
			forwardInTime();
		}
	}

	void recallThreshold(Number<Kernel> t) {

		assert(building_batch == nullptr);

		while (history.size() >= 1 && history.last()->post_maxcost > t) {
			backInTime();
		}
		while (!undone.empty() && undone.last()->post_maxcost <= t) {
			forwardInTime();
		}
	}

	void backInTime() {

		assert(building_batch == nullptr);

		Batch* batch = history.back();
		history.pop_back();
		undone.push_back(batch);

		batch->undo(graph);
	}

	void forwardInTime() {

		assert(building_batch == nullptr);

		Batch* batch = undone.back();
		undone.pop_back();
		history.push_back(batch);

		batch->redo(graph);
	}

	void goToPresent() {
		while (!atPresent()) {
			forwardInTime();
		}
	}

	bool atPresent() {
		return undone.empty();
	}

	void startBatch(Number<Kernel> c) {
		assert(building_batch == nullptr);
		assert(atPresent());

		building_batch = new Batch();
		history.push_back(building_batch);

		if (max_cost < c) {
			max_cost = c;
		}

		building_batch->post_maxcost = max_cost;
	}

	void endBatch() {
		assert(building_batch != nullptr);

		building_batch->post_complexity = graph.getEdgeCount();
		building_batch = nullptr;
	}
};

} // namespace cartocrow::simplification