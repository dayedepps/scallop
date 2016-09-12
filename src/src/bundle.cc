#include <cassert>
#include <cstdio>
#include <map>
#include <iomanip>
#include <fstream>

#include "bundle.h"
#include "binomial.h"
#include "region.h"
#include "config.h"
#include "util.h"

bundle::bundle(const bundle_base &bb)
	: bundle_base(bb)
{
}

bundle::~bundle()
{}

int bundle::build()
{
	compute_strand();
	check_left_ascending();
	build_junctions();

	if(junctions.size() <= 0 && ignore_single_exon_transcripts == true) return 0;

	build_junction_graph();
	//draw_junction_graph("jr.tex");

	build_regions();
	build_partial_exons();
	build_partial_exon_map();
	link_partial_exons();
	build_splice_graph();

	extend_isolated_start_boundaries();
	extend_isolated_end_boundaries();
	//identify_boundary_edges();

	build_hyper_edges2();

	/*
	build_segments();
	update_partial_exons();
	build_partial_exon_map();
	link_partial_exons();
	build_splice_graph();
	build_hyper_edges2();
	*/

	return 0;
}

int bundle::compute_strand()
{
	int n0 = 0, np = 0, nq = 0;
	for(int i = 0; i < hits.size(); i++)
	{
		if(hits[i].xs == '.') n0++;
		if(hits[i].xs == '+') np++;
		if(hits[i].xs == '-') nq++;
	}

	if(np > nq) strand = '+';
	if(np < nq) strand = '-';

	return 0;
}

int bundle::check_left_ascending()
{
	for(int i = 1; i < hits.size(); i++)
	{
		int32_t p1 = hits[i - 1].pos;
		int32_t p2 = hits[i].pos;
		assert(p1 <= p2);
	}
	return 0;
}

int bundle::check_right_ascending()
{
	for(int i = 1; i < hits.size(); i++)
	{
		int32_t p1 = hits[i - 1].rpos;
		int32_t p2 = hits[i].rpos;
		assert(p1 <= p2);
	}
	return 0;
}

int bundle::build_junctions()
{
	map<int64_t, int> m;
	vector<int64_t> v;
	for(int i = 0; i < hits.size(); i++)
	{
		hits[i].get_splice_positions(v);
		if(v.size() == 0) continue;
		for(int k = 0; k < v.size(); k++)
		{
			int64_t p = v[k];
			if(m.find(p) == m.end()) m.insert(pair<int64_t, int>(p, 1));
			else m[p]++;
		}
	}

	map<int64_t, int>::iterator it;
	for(it = m.begin(); it != m.end(); it++)
	{
		if(it->second < min_splice_boundary_hits) continue;
		junctions.push_back(junction(it->first, it->second));
	}
	return 0;
}

int bundle::build_junction_graph()
{
	jr.clear();

	MPI s;
	s.insert(PI(lpos, START_BOUNDARY));
	s.insert(PI(rpos, END_BOUNDARY));
	for(int i = 0; i < junctions.size(); i++)
	{
		int32_t l = junctions[i].lpos;
		int32_t r = junctions[i].rpos;

		if(s.find(l) == s.end()) s.insert(PI(l, LEFT_SPLICE));
		else if(s[l] == RIGHT_SPLICE) s[l] = LEFT_RIGHT_SPLICE;

		if(s.find(r) == s.end()) s.insert(PI(r, RIGHT_SPLICE));
		else if(s[r] == LEFT_SPLICE) s[r] = LEFT_RIGHT_SPLICE;
	}

	vector<PPI> v(s.begin(), s.end());
	sort(v.begin(), v.end());

	// position to vertex
	MPI p2v;
	for(int i = 0; i < v.size(); i++)
	{
		p2v.insert(PI(v[i].first, i));
	}

	// vertices
	jr.clear();
	for(int i = 0; i < v.size(); i++)
	{
		jr.add_vertex();
		vertex_info vi;
		vi.pos = v[i].first;
		vi.type = v[i].second;
		jr.set_vertex_info(i, vi);
	}

	// edges: connecting adjacent regions
	for(int i = 0; i < v.size() - 1; i++)
	{
		edge_descriptor p = jr.add_edge(i, i + 1);
		edge_info ei;
		ei.jid = -1;
		jr.set_edge_info(p, ei);
	}

	// edges: each junction
	for(int i = 0; i < junctions.size(); i++)
	{
		int32_t l = junctions[i].lpos;
		int32_t r = junctions[i].rpos;
		assert(p2v.find(l) != p2v.end());
		assert(p2v.find(r) != p2v.end());
		edge_descriptor p = jr.add_edge(p2v[l], p2v[r]);
		edge_info ei;
		ei.jid = i;
		jr.set_edge_info(p, ei);
	}

	return 0;
}

int bundle::draw_junction_graph(const string &file)
{
	char buf[10240];

	MIS mis;
	for(int i = 0; i < jr.num_vertices(); i++)
	{
		int32_t p = jr.get_vertex_info(i).pos;
		sprintf(buf, "%d", p);
		mis.insert(PIS(i, buf));
	}

	MES mes;
	edge_iterator it1, it2;
	for(tie(it1, it2) = jr.edges(); it1 != it2; it1++)
	{
		int jid = jr.get_edge_info(*it1).jid;
		sprintf(buf, "%d", jid);
		mes.insert(PES(*it1, buf));
	}

	jr.draw(file, mis, mes, 3.0);
	return 0;
}

int bundle::test_junction_graph()
{
	for(int i = 0; i < jr.num_vertices(); i++)
	{
		for(int j = i + 1; j < jr.num_vertices(); j++)
		{
			VE ve;
			int p = traverse_junction_graph1(i, j, ve);
			printf("path1 from %d to %d, length = %d :", i, j, p);
			for(int k = 0; k < ve.size(); k++)
			{
				int s = ve[k]->source();
				int t = ve[k]->target();
				int id = jr.get_edge_info(ve[k]).jid;
				printf("(%d, %d, %d) <- ", s, t, id);
			}
			printf(" START\n");
		}
	}
	return 0;
}

int bundle::search_junction_graph(int32_t p)
{
	assert(jr.num_vertices() >= 2);
	int l = 0;
	int r = jr.num_vertices() - 1;
	while(l < r)
	{
		int m = (l + r) / 2;
		int32_t p1 = jr.get_vertex_info(m).pos;
		int32_t p2 = jr.get_vertex_info(m + 1).pos;
		assert(p1 < p2);
		if(p >= p1 && p < p2) return m;
		if(p < p1) r = m;
		if(p >= p2) l = m + 1;
	}
	return -1;
}

int bundle::traverse_junction_graph1(int s, int t)
{
	VE ve;
	return traverse_junction_graph1(s, t, ve);
}

int bundle::traverse_junction_graph1(int s, int t, VE &ve)
{
	ve.clear();
	if(s > t) return -1;

	vector<double> v1, v2;		// shortest path/with one edge
	VE ve1, ve2;				// tracing back pointers
	v1.push_back(0);
	v2.push_back(-1);
	ve1.push_back(null_edge);
	ve2.push_back(null_edge);

	for(int k = s + 1; k <= t; k++)
	{
		edge_descriptor ee1 = null_edge;
		edge_descriptor ee2 = null_edge;
		double ww1 = DBL_MAX;
		double ww2 = DBL_MAX;
		edge_iterator it1, it2;
		for(tie(it1, it2) = jr.in_edges(k); it1 != it2; it1++)
		{
			int ss = (*it1)->source();
			if(ss < s) continue;

			double w = 0;
			int id = jr.get_edge_info(*it1).jid;
			if(id < 0) 
			{
				assert(ss == k - 1);
				w = jr.get_vertex_info(k).pos - jr.get_vertex_info(ss).pos;

				if(v1[ss - s] + w < ww1)
				{
					ww1 = v1[ss - s] + w;
					ee1 = *it1;
				}

				if(v1[ss - s] + w < ww2)
				{
					ww2 = v1[ss - s] + w;
					ee2 = *it1;
				}
			}
			else
			{
				if(v1[ss - s] < ww1)
				{
					ww1 = v1[ss - s];
					ee1 = *it1;
				}

				if(v2[ss - s] >= 0 && v2[ss - s] < ww2)
				{
					ww2 = v2[ss - s];
					ee2 = *it1;
				}
			}
		}

		assert(ee1 != null_edge);
		v1.push_back(ww1);
		ve1.push_back(ee1);

		if(ee2 == null_edge)
		{
			v2.push_back(-1);
			ve2.push_back(null_edge);
		}
		else
		{
			v2.push_back(ww2);
			ve2.push_back(ee2);
		}
	}

	if(v2[t - s] <= 0) return -1;

	int k = t - s;
	while(ve2[k] != null_edge)
	{
		ve.push_back(ve2[k]);
		int id = jr.get_edge_info(ve2[k]).jid;
		k = ve2[k]->source() - s;
		if(id < 0) break;
	}

	while(ve1[k] != null_edge)
	{
		ve.push_back(ve1[k]);
		k = ve1[k]->source() - s;
	}

	return (int)(v2[t - s]);
}

int bundle::traverse_junction_graph(int s, int t, VE &ve)
{
	ve.clear();
	if(s > t) return -1;
	vector<double> v1;
	VE v2;
	v1.push_back(0);
	v2.push_back(null_edge);

	for(int k = s + 1; k <= t; k++)
	{
		edge_descriptor ee = null_edge;
		double ww = DBL_MAX;
		edge_iterator it1, it2;
		for(tie(it1, it2) = jr.in_edges(k); it1 != it2; it1++)
		{
			int ss = (*it1)->source();
			if(ss < s) continue;

			double w = 0;
			int id = jr.get_edge_info(*it1).jid;
			if(id < 0) 
			{
				assert(ss == k - 1);
				w = jr.get_vertex_info(k).pos - jr.get_vertex_info(ss).pos;
			}

			if(v1[ss - s] + w < ww)
			{
				ww = v1[ss - s] + w;
				ee =  *it1;
			}
		}

		assert(ee != null_edge);
		v1.push_back(ww);
		v2.push_back(ee);
	}

	int k = t - s;
	while(v2[k] != null_edge)
	{
		ve.push_back(v2[k]);
		k = v2[k]->source() - s;
	}
	return (int)(v1[t - s]);
}

int bundle::align_hits()
{
	mmap.clear();
	imap.clear();
	for(int i = 0; i < hits.size(); i++)
	{
		hit &h = hits[i];
		vector<int64_t> vm;
		vector<int64_t> vi;
		vector<int64_t> vd;

		h.get_mid_intervals(vm, vi, vd);

		for(int k = 0; k < vm.size(); k++)
		{
			int32_t s = high32(vm[k]);
			int32_t t = low32(vm[k]);
			mmap += make_pair(ROI(s, t), 1);
		}

		for(int k = 0; k < vi.size(); k++)
		{
			int32_t s = high32(vi[k]);
			int32_t t = low32(vi[k]);
			imap += make_pair(ROI(s, t), 1);
		}

		for(int k = 0; k < vd.size(); k++)
		{
			int32_t s = high32(vd[k]);
			int32_t t = low32(vd[k]);
			imap += make_pair(ROI(s, t), 1);
		}
	}

	// Test imap
	/*
	SIMI::iterator jit;
	for(jit = imap.begin(); jit != imap.end(); jit++)
	{
		printf("imap (%d, %d) -> %d\n", lower(jit->first), upper(jit->first), jit->second);
	}
	*/

	return 0;
}

int bundle::build_regions()
{
	regions.clear();
	for(int k = 0; k < jr.num_vertices() - 1; k++)
	{
		int32_t lpos = jr.get_vertex_info(k).pos;
		int32_t rpos = jr.get_vertex_info(k + 1).pos;

		int ltype = jr.get_vertex_info(k).type; 
		int rtype = jr.get_vertex_info(k + 1).type; 

		if(ltype == LEFT_RIGHT_SPLICE) ltype = RIGHT_SPLICE;
		if(rtype == LEFT_RIGHT_SPLICE) rtype = LEFT_SPLICE;

		regions.push_back(region(lpos, rpos, ltype, rtype, &mmap, &imap));
	}

	return 0;
}

int bundle::build_partial_exons()
{
	pexons.clear();
	for(int i = 0; i < regions.size(); i++)
	{
		region &r = regions[i];
		pexons.insert(pexons.end(), r.pexons.begin(), r.pexons.end());
	}
	return 0;
}

int bundle::build_partial_exon_map()
{
	pmap.clear();
	for(int i = 0; i < pexons.size(); i++)
	{
		partial_exon &p = pexons[i];
		pmap += make_pair(ROI(p.lpos, p.rpos), i + 1);
	}
	return 0;
}

int bundle::locate_left_partial_exon(int32_t x)
{
	SIMI it = pmap.find(ROI(x, x + 1));
	if(it == pmap.end()) return -1;
	assert(it->second >= 1);
	assert(it->second <= pexons.size());

	int k = it->second - 1;
	int32_t p1 = lower(it->first);
	int32_t p2 = upper(it->first);
	assert(p2 >= x);
	assert(p1 <= x);

	if(x - p1 > min_flank_length && p2 - x < min_flank_length) k++;
	if(k >= pexons.size()) return -1;
	return k;
}

int bundle::locate_right_partial_exon(int32_t x)
{
	SIMI it = pmap.find(ROI(x - 1, x));
	if(it == pmap.end()) return -1;
	assert(it->second >= 1);
	assert(it->second <= pexons.size());

	int k = it->second - 1;
	int32_t p1 = lower(it->first);
	int32_t p2 = upper(it->first);
	assert(p1 < x);
	assert(p2 >= x);

	if(p2 - x > min_flank_length && x - p1 <= min_flank_length) k--;
	return k;
}

int bundle::build_hyper_edges1()
{
	hs.clear();
	for(int i = 0; i < hits.size(); i++)
	{
		hit &h = hits[i];
		if((h.flag & 0x4) >= 1) continue;

		vector<int64_t> v;
		h.get_matched_intervals(v);
		if(v.size() == 0) continue;

		set<int> sp;
		for(int k = 0; k < v.size(); k++)
		{
			int32_t p1 = high32(v[k]);
			int32_t p2 = low32(v[k]);

			int k1 = locate_left_partial_exon(p1);
			int k2 = locate_right_partial_exon(p2);
			if(k1 < 0 || k2 < 0) continue;

			for(int j = k1; j <= k2; j++) sp.insert(j);
		}

		if(sp.size() <= 1) continue;
		hs.add_node_list(sp);
	}

	return 0;
}

int bundle::build_hyper_edges2()
{
	sort(hits.begin(), hits.end(), hit_compare_by_name);

	hs.clear();

	string qname;
	vector<int> sp;
	for(int i = 0; i < hits.size(); i++)
	{
		hit &h = hits[i];
		
		/*
		printf("sp = ( ");
		printv(sp);
		printf(")\n");
		h.print();
		*/

		if(h.qname != qname)
		{
			set<int> s(sp.begin(), sp.end());
			if(s.size() >= 2) hs.add_node_list(s);
			sp.clear();
		}

		qname = h.qname;

		if((h.flag & 0x4) >= 1) continue;

		vector<int64_t> v;
		h.get_matched_intervals(v);

		vector<int> sp2;
		for(int k = 0; k < v.size(); k++)
		{
			int32_t p1 = high32(v[k]);
			int32_t p2 = low32(v[k]);

			int k1 = locate_left_partial_exon(p1);
			int k2 = locate_right_partial_exon(p2);
			if(k1 < 0 || k2 < 0) continue;

			for(int j = k1; j <= k2; j++) sp2.push_back(j);
		}

		if(sp.size() >= 1 && sp2.size() >= 1 && sp.back() + 1 < sp2[0]) sp.clear();
		sp.insert(sp.end(), sp2.begin(), sp2.end());
	}

	return 0;
}

int bundle::link_partial_exons()
{
	if(pexons.size() == 0) return 0;

	MPI lm;
	MPI rm;
	for(int i = 0; i < pexons.size(); i++)
	{
		int32_t l = pexons[i].lpos;
		int32_t r = pexons[i].rpos;

		assert(lm.find(l) == lm.end());
		assert(rm.find(r) == rm.end());
		lm.insert(PPI(l, i));
		rm.insert(PPI(r, i));
	}

	for(int i = 0; i < junctions.size(); i++)
	{
		junction &b = junctions[i];
		MPI::iterator li = rm.find(b.lpos);
		MPI::iterator ri = lm.find(b.rpos);

		assert(li != rm.end());
		assert(ri != lm.end());

		if(li != rm.end() && ri != lm.end())
		{
			b.lexon = li->second;
			b.rexon = ri->second;
		}
		else
		{
			b.lexon = b.rexon = -1;
		}
	}
	return 0;
}

int bundle::build_splice_graph()
{
	gr.clear();

	// vertices: start, each region, end
	gr.add_vertex();
	vertex_info vi0;
	vi0.lpos = lpos;
	vi0.rpos = lpos;
	gr.set_vertex_weight(0, 0);
	gr.set_vertex_info(0, vi0);
	for(int i = 0; i < pexons.size(); i++)
	{
		const partial_exon &r = pexons[i];
		int length = r.rpos - r.lpos;
		assert(length >= 1);
		gr.add_vertex();
		gr.set_vertex_weight(i + 1, r.ave < 1.0 ? 1.0 : r.ave);
		vertex_info vi;
		vi.lpos = r.lpos;
		vi.rpos = r.rpos;
		vi.length = length;
		vi.stddev = r.dev < 1.0 ? 1.0 : r.dev;
		//vi.adjust = r.adjust;
		gr.set_vertex_info(i + 1, vi);
	}

	gr.add_vertex();
	vertex_info vin;
	vin.lpos = rpos;
	vin.rpos = rpos;
	gr.set_vertex_weight(pexons.size() + 1, 0);
	gr.set_vertex_info(pexons.size() + 1, vin);

	// edges: each junction => and e2w
	for(int i = 0; i < junctions.size(); i++)
	{
		const junction &b = junctions[i];

		if(b.lexon < 0 || b.rexon < 0) continue;

		const partial_exon &x = pexons[b.lexon];
		const partial_exon &y = pexons[b.rexon];

		edge_descriptor p = gr.add_edge(b.lexon + 1, b.rexon + 1);
		assert(b.count >= 1);
		edge_info ei;
		ei.weight = b.count;
		gr.set_edge_info(p, ei);
		gr.set_edge_weight(p, b.count);
	}

	// edges: connecting start/end and pexons
	int ss = 0;
	int tt = pexons.size() + 1;
	for(int i = 0; i < pexons.size(); i++)
	{
		const partial_exon &r = pexons[i];

		if(r.ltype == START_BOUNDARY)
		{
			edge_descriptor p = gr.add_edge(ss, i + 1);
			double w = r.ave;
			if(i >= 1 && pexons[i - 1].rpos == r.lpos) w -= pexons[i - 1].ave;
			if(w < 1.0) w = 1.0;
			gr.set_edge_weight(p, w);
			edge_info ei;
			ei.weight = w;
			gr.set_edge_info(p, ei);
		}

		if(r.rtype == END_BOUNDARY) 
		{
			edge_descriptor p = gr.add_edge(i + 1, tt);
			double w = r.ave;
			if(i < pexons.size() - 1 && pexons[i + 1].lpos == r.rpos) w -= pexons[i + 1].ave;
			if(w < 1.0) w = 1.0;
			gr.set_edge_weight(p, w);
			edge_info ei;
			ei.weight = w;
			gr.set_edge_info(p, ei);
		}
	}

	// edges: connecting adjacent pexons => e2w
	for(int i = 0; i < (int)(pexons.size()) - 1; i++)
	{
		const partial_exon &x = pexons[i];
		const partial_exon &y = pexons[i + 1];

		if(x.rpos != y.lpos) continue;

		assert(x.rpos == y.lpos);
		
		// TODO
		int xd = gr.out_degree(i + 1);
		int yd = gr.in_degree(i + 2);
		double wt = (xd < yd) ? x.ave : y.ave;
		//int32_t xr = compute_overlap(mmap, x.rpos - 1);
		//int32_t yl = compute_overlap(mmap, y.lpos);
		//double wt = xr < yl ? xr : yl;

		edge_descriptor p = gr.add_edge(i + 1, i + 2);
		double w = (wt < 1.0) ? 1.0 : wt;
		gr.set_edge_weight(p, w);
		edge_info ei;
		ei.weight = w;
		gr.set_edge_info(p, ei);
	}


	return 0;
}

int bundle::split_partial_exons()
{
	vector<partial_exon> vv;
	for(int i = 0; i < pexons.size(); i++)
	{
		partial_exon &pe = pexons[i];
		int n = (pe.rpos - pe.lpos) / partial_exon_length;

		if(n <= 1)
		{
			vv.push_back(pe);
			continue;
		}

		int32_t len = (pe.rpos - pe.lpos) / n;
		for(int k = 0; k < n; k++)
		{
			int lltype = (k == 0) ? pe.ltype : MIDDLE_CUT;
			int rrtype = (k == n - 1) ? pe.rtype : MIDDLE_CUT;

			int32_t p1 = pe.lpos + k * len;
			int32_t p2 = pe.lpos + (k + 1) * len;
			assert(p2 <= pe.rpos);
			if(k == n - 1) p2 = pe.rpos;

			partial_exon p(p1, p2, lltype, rrtype);
			evaluate_rectangle(mmap, p.lpos, p.rpos, p.ave, p.dev);
			vv.push_back(p);
		}
	}

	pexons = vv;
	return 0;
}

int bundle::assign_edge_info_weights()
{
	edge_iterator it1, it2;
	for(tie(it1, it2) = gr.edges(); it1 != it2; it1++)
	{
		edge_info ei = gr.get_edge_info(*it1);
		ei.weight = 0;
		gr.set_edge_info(*it1, ei);
	}

	MED med;
	for(MVII::iterator it = hs.nodes.begin(); it != hs.nodes.end(); it++)
	{
		vector<int> v = it->first;
		int c = it->second;
		for(int k = 0; k < v.size() - 1; k++)
		{
			int s = v[k];
			int t = v[k + 1];
			PEB p = gr.edge(s, t);
			if(p.second == false) continue;

			//printf("add weight %d to (%d, %d)\n", c, s, t);

			if(med.find(p.first) == med.end()) med.insert(PED(p.first, c));
			else med[p.first] += c;
		}
	}

	for(MED::iterator it = med.begin(); it != med.end(); it++)
	{
		edge_descriptor e = it->first;
		double w = it->second;
		edge_info ei = gr.get_edge_info(e);
		ei.weight = w;
		gr.set_edge_info(e, ei);
	}
	return 0;
}

int bundle::build_segments()
{
	int k = 1;
	while(true)
	{
		if(k >= gr.num_vertices() - 1) break;
		segment s(&mmap);
		build_segment(s, k);
		assert(s.size() >= 1);
		segments.push_back(s);
		k += s.size();
	}

	for(int i = 0; i < segments.size(); i++)
	{
		segments[i].build();
	}
	return 0;
}

int bundle::build_segment(segment &s, int k)
{
	if(k == 0) return 0;
	if(k >= gr.num_vertices() - 1) return 0;

	s.add_partial_exon(k - 1, pexons[k - 1], 0);
	if(pexons[k - 1].ltype == START_BOUNDARY) return 0;
	if(pexons[k - 1].rtype == END_BOUNDARY) return 0;
	if(k >= gr.num_vertices() - 2) return 0;
	if(gr.out_degree(k) >= 3) return 0;
	if(gr.out_degree(k) <= 0) return 0;

	if(gr.out_degree(k) == 2)
	{
		if(k + 2 >= gr.num_vertices() - 1) return 0;
		if(pexons[k + 1].rtype == END_BOUNDARY) return 0;

		int32_t p1 = pexons[k - 1].rpos;
		int32_t p2 = pexons[k].lpos;
		int32_t p3 = pexons[k].rpos;
		int32_t p4 = pexons[k + 1].lpos;
		if(p1 != p2) return 0;
		if(p3 != p4) return 0;

		PEB eb1 = gr.edge(k, k + 1);
		PEB eb2 = gr.edge(k, k + 2);
		PEB eb3 = gr.edge(k + 1, k + 2);
		if(eb1.second == false) return 0;
		if(eb2.second == false) return 0;
		if(eb3.second == false) return 0;
		if(gr.in_degree(k + 2) > 2) return 0;

		double w = gr.get_edge_weight(eb2.first);
		s.add_partial_exon(k, pexons[k], w);
		s.add_partial_exon(k + 1, pexons[k + 1], 0);

		return 0;
		//return build_segment(s, k + 2);
	}

	if(gr.out_degree(k) == 1)
	{
		PEB eb1 = gr.edge(k, k + 1);
		if(eb1.second == false) return 0;
		if(gr.in_degree(k + 1) >= 2) return 0;
		if(pexons[k].rtype == END_BOUNDARY) return 0;

		/*
		int32_t p1 = pexons[k - 1].rpos;
		int32_t p2 = pexons[k].lpos;
		if(p1 != p2) return 0;
		*/

		s.add_partial_exon(k, pexons[k], 0);
		return 0;
		//return build_segment(s, k + 1);
	}

	return 0;
}

int bundle::update_partial_exons()
{
	pexons.clear();
	for(int i = 0; i < segments.size(); i++)
	{
		segment &s = segments[i];
		pexons.insert(pexons.end(), s.pexons.begin(), s.pexons.end());
	}
	return 0;
}

int bundle::identify_boundary_edges()
{
	while(true)
	{
		bool b1 = identify_5end();
		bool b2 = identify_5end();
		if(b1 == false && b2 == false) break;
	}
	return 0;
}

bool bundle::identify_5end()
{
	double score5 = -1, sigma5 = -1;
	int k5 = -1; 
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		double score, sigma;
		identify_5end(i, score, sigma);
		if(score <= score5) continue;
		if(score < 600) continue;
		if(sigma < 10) continue;
		score5 = score;
		sigma5 = sigma;
		k5 = i;
	}
	if(k5 == -1) return false;

	printf("maximum 5end vertex = %d, score = %.2lf, sigma = %.2lf, pos = %d-%d\n", 
			k5, score5, sigma5, gr.get_vertex_info(k5).lpos, gr.get_vertex_info(k5).rpos);

	edge_descriptor e = gr.add_edge(0, k5);
	double w = gr.get_vertex_weight(k5) - gr.get_vertex_weight(k5 - 1);
	if(w <= 1.0) w = 1.0;
	gr.set_edge_weight(e, w);
	gr.set_edge_info(e, edge_info());

	return true;
}

bool bundle::identify_3end()
{
	double score3 = -1, sigma3 = -1;
	int k3 = -1;
	for(int i = 1; i < gr.num_vertices() - 1; i++)
	{
		double score, sigma;
		identify_3end(i, score, sigma);
		if(score <= score3) continue;
		if(score < 600) continue;
		if(sigma < 10) continue;
		score3 = score;
		sigma3 = sigma;
		k3 = i;
	}

	if(k3 == -1) return false;

	printf("maximum 3end vertex = %d, score = %.2lf, sigma = %.2lf, pos = %d-%d\n", 
			k3, score3, sigma3, gr.get_vertex_info(k3).lpos, gr.get_vertex_info(k3).rpos);

	edge_descriptor e = gr.add_edge(k3, gr.num_vertices() - 1);
	double w = gr.get_vertex_weight(k3) - gr.get_vertex_weight(k3 + 1);
	if(w <= 1.0) w = 1.0;
	gr.set_edge_weight(e, w);
	gr.set_edge_info(e, edge_info());

	return true;
}

int bundle::extend_isolated_end_boundaries()
{
	for(int i = 1; i < gr.num_vertices(); i++)
	{
		if(gr.in_degree(i) != 1) continue;
		if(gr.out_degree(i) != 1) continue;

		edge_iterator it1, it2;
		tie(it1, it2) = gr.in_edges(i);
		edge_descriptor e1 = (*it1);
		tie(it1, it2) = gr.out_edges(i);
		edge_descriptor e2 = (*it1);
		int s = e1->source();
		int t = e2->target();

		if(gr.out_degree(s) != 1) continue;
		if(t != gr.num_vertices() - 1) continue;
		if(gr.get_edge_weight(e1) >= 1.5) continue;
		//if(gr.get_edge_weight(e2) >= 1.5) continue;
		if(gr.get_vertex_weight(s) <= 5.0) continue;
		if(gr.get_vertex_info(s).rpos == gr.get_vertex_info(i).lpos) continue;

		double w = gr.get_vertex_weight(s) - gr.get_edge_weight(e1);
		edge_descriptor e = gr.add_edge(s, t);
		gr.set_edge_weight(e, w);
		gr.set_edge_info(e, edge_info());
		
		//printf("extend isolated end boundary (%d, %d) -> %d\n", k1, k2, n);
	}
	return 0;
}

int bundle::extend_isolated_start_boundaries()
{
	for(int i = 1; i < gr.num_vertices(); i++)
	{
		if(gr.in_degree(i) != 1) continue;
		if(gr.out_degree(i) != 1) continue;

		edge_iterator it1, it2;
		tie(it1, it2) = gr.in_edges(i);
		edge_descriptor e1 = (*it1);
		tie(it1, it2) = gr.out_edges(i);
		edge_descriptor e2 = (*it1);
		int s = e1->source();
		int t = e2->target();

		if(s != 0) continue;
		if(gr.in_degree(t) != 1) continue;
		//if(gr.get_edge_weight(e1) >= 1.5) continue;
		if(gr.get_edge_weight(e2) >= 1.5) continue;
		if(gr.get_vertex_weight(t) <= 5.0) continue;
		if(gr.get_vertex_info(i).rpos == gr.get_vertex_info(t).lpos) continue;

		double w = gr.get_vertex_weight(t) - gr.get_edge_weight(e2);
		edge_descriptor e = gr.add_edge(s, t);
		gr.set_edge_weight(e, w);
		gr.set_edge_info(e, edge_info());
		
		//printf("extend isolated start boundary (%d, %d) -> %d\n", k1, k2, n);
	}
	return 0;
}

int bundle::identify_5end(int x, double &score, double &sigma)
{
	score = sigma = -1;
	if(x <= 1) return 0;
	if(gr.edge(0, x - 1).second == true) return 0;
	if(gr.edge(0, x).second == true) return 0;
	if(gr.edge(x - 1, x).second == false) return 0;
	if(gr.in_degree(x) >= 2) return 0;
	if(gr.out_degree(x - 1) >= 2) return 0;

	vertex_info v1 = gr.get_vertex_info(x - 1);
	vertex_info v2 = gr.get_vertex_info(x);
	int l1 = v1.rpos - v1.lpos;
	int l2 = v2.rpos - v2.lpos;

	if(l1 <= 50) return 0;
	if(l2 <= 50) return 0;

	double w1 = gr.get_vertex_weight(x - 1);
	double w2 = gr.get_vertex_weight(x);
	int x1 = w1 * l1 / average_read_length;
	int x2 = w2 * l2 / average_read_length;
	
	if(x1 + x2 <= 10) return 0;

	double r = l2 * 1.0 / (l1 + l2);
	score = compute_binomial_score(x1 + x2, r, x2);
	sigma = (w2 - w1) / v1.stddev;

	return 0;
}

int bundle::identify_3end(int x, double &score, double &sigma)
{
	score = sigma = -1;
	int n = gr.num_vertices() - 1;
	if(x >= n - 1) return 0;
	if(gr.edge(x, n).second == true) return 0;
	if(gr.edge(x + 1, n).second == true) return 0;
	if(gr.edge(x, x + 1).second == false) return 0;
	if(gr.out_degree(x) >= 2) return 0;
	if(gr.in_degree(x + 1) >= 2) return 0;

	vertex_info v1 = gr.get_vertex_info(x);
	vertex_info v2 = gr.get_vertex_info(x + 1);
	int l1 = v1.rpos - v1.lpos;
	int l2 = v2.rpos - v2.lpos;

	if(l1 <= 50) return 0;
	if(l2 <= 50) return 0;

	double w1 = gr.get_vertex_weight(x);
	double w2 = gr.get_vertex_weight(x + 1);
	int x1 = w1 * l1 / average_read_length;
	int x2 = w2 * l2 / average_read_length;
	
	if(x1 + x2 <= 10) return 0;

	double r = l1 * 1.0 / (l1 + l2);
	score = compute_binomial_score(x1 + x2, r, x1);
	sigma = (w1 - w2) / v2.stddev;

	return 0;
}


int bundle::print(int index)
{
	printf("\nBundle %d: ", index);

	// statistic xs
	int n0 = 0, np = 0, nq = 0;
	for(int i = 0; i < hits.size(); i++)
	{
		if(hits[i].xs == '.') n0++;
		if(hits[i].xs == '+') np++;
		if(hits[i].xs == '-') nq++;
	}

	printf("tid = %d, #hits = %lu, #partial-exons = %lu, range = %s:%d-%d, orient = %c (%d, %d, %d)\n",
			tid, hits.size(), pexons.size(), chrm.c_str(), lpos, rpos, strand, n0, np, nq);

	// print hits
	/*
	for(int i = 0; i < hits.size(); i++)
	{
		hits[i].print();
	}
	*/

	// print regions
	for(int i = 0; i < regions.size(); i++)
	{
		regions[i].print(i);
	}

	// print junctions 
	for(int i = 0; i < junctions.size(); i++)
	{
		junctions[i].print(i);
	}

	// print partial exons
	for(int i = 0; i < pexons.size(); i++)
	{
		pexons[i].print(i);
	}

	// print hyper-edges
	//hs.print();

	// print segments
	//for(int i = 0; i < segments.size(); i++) segments[i].print(i);

	// print clips
	/*
	for(int i = 0; i < lsoft.size(); i++) printf("left  soft clips: pos = %s:%d, counts = %d\n", chrm.c_str(), lsoft[i].first, lsoft[i].second);
	for(int i = 0; i < rsoft.size(); i++) printf("right soft clips: pos = %s:%d, counts = %d\n", chrm.c_str(), rsoft[i].first, rsoft[i].second);
	for(int i = 0; i < lhard.size(); i++) printf("left  hard clips: pos = %s:%d, counts = %d\n", chrm.c_str(), lhard[i].first, lhard[i].second);
	for(int i = 0; i < rhard.size(); i++) printf("right hard clips: pos = %s:%d, counts = %d\n", chrm.c_str(), rhard[i].first, rhard[i].second);
	*/


	return 0;
}

int bundle::output_transcripts(ofstream &fout, const vector<path> &p, const string &gid) const
{
	for(int i = 0; i < p.size(); i++)
	{
		string tid = gid + "." + tostring(i);
		output_transcript(fout, p[i], gid, tid);
	}
	return 0;
}

int bundle::output_transcript(ofstream &fout, const path &p, const string &gid, const string &tid) const
{
	fout.precision(2);
	fout<<fixed;

	const vector<int> &v = p.v;
	double abd = p.abd;

	assert(v[0] == 0);
	assert(v[v.size() - 1] == pexons.size() + 1);
	if(v.size() < 2) return 0;

	int ss = v[1];
	int tt = v[v.size() - 2];
	int32_t ll = pexons[ss - 1].lpos;
	int32_t rr = pexons[tt - 1].rpos;

	fout<<chrm.c_str()<<"\t";		// chromosome name
	fout<<algo.c_str()<<"\t";		// source
	fout<<"transcript\t";			// feature
	fout<<ll + 1<<"\t";				// left position
	fout<<rr<<"\t";					// right position
	fout<<1000<<"\t";				// score, now as abundance
	fout<<strand<<"\t";				// strand
	fout<<".\t";					// frame
	fout<<"gene_id \""<<gid.c_str()<<"\"; ";
	fout<<"transcript_id \""<<tid.c_str()<<"\"; ";
	fout<<"expression \""<<abd<<"\";"<<endl;

	join_interval_map jmap;
	for(int k = 1; k < v.size() - 1; k++)
	{
		const partial_exon &r = pexons[v[k] - 1];
		jmap += make_pair(ROI(r.lpos, r.rpos), 1);
	}

	int cnt = 0;
	for(JIMI it = jmap.begin(); it != jmap.end(); it++)
	{
		fout<<chrm.c_str()<<"\t";			// chromosome name
		fout<<algo.c_str()<<"\t";			// source
		fout<<"exon\t";						// feature
		fout<<lower(it->first) + 1<<"\t";	// left position
		fout<<upper(it->first)<<"\t";		// right position
		fout<<1000<<"\t";					// score
		fout<<strand<<"\t";					// strand
		fout<<".\t";						// frame
		fout<<"gene_id \""<<gid.c_str()<<"\"; ";
		fout<<"transcript_id \""<<tid.c_str()<<"\"; ";
		fout<<"exon_number \""<<++cnt<<"\"; ";
		fout<<"expression \""<<abd<<"\";"<<endl;
	}
	return 0;
}


