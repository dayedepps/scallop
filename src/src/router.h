#ifndef __ROUTER_H__
#define __ROUTER_H__

#include <vector>
#include "util.h"
#include "splice_graph.h"
#include "undirected_graph.h"

using namespace std;

class router
{
public:
	router(int r, splice_graph &g, MEI &ei, VE &ie);

public:
	int root;					// central vertex
	splice_graph &gr;			// reference splice graph
	MEI &e2i;					// reference map of edge to index
	VE &i2e;					// reference map of index to edge

	vector<PI> routes;			// pairs of connections
	vector<double> counts;		// reads spanning each route
	MI e2u;						// edge to index
	vector<int> u2e;			// index to edge

	double pratio;				// partition ratio
	double pvalue;				// partition absolute difference
	vector<int> pv1;			// partition 1, left side
	vector<int> pv2;			// partition 1, right side
	vector<int> qv1;			// partition 2, left side
	vector<int> qv2;			// partition 2, right side

public:
	// build indices
	int build_indices();

	// divide
	int divide();
	int run_ilp1();					// with multiplier ratio
	int run_ilp2();					// with difference ratio
	int evaluate_partition();		// compute pvalue and pratio

	// modify routes
	int add_route(const PI &p, double c);
	int remove_route(const PI &p);
	int replace_in_edge(int ex, int ey);
	int replace_out_edge(int ex, int ey);
	int split_in_edge(int ex, int ey, double r);
	int split_out_edge(int ex, int ey, double r);
	int remove_in_edges(const vector<int> &v);
	int remove_out_edges(const vector<int> &v);
	int remove_in_edge(int x);
	int remove_out_edge(int x);

	// print and stats
	int print() const;
	double total_counts() const;
};

#endif