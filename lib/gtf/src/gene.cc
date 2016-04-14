#include <map>
#include <algorithm>
#include <cassert>
#include "gene.h"

int gene::add_transcript(const transcript &t)
{
	transcripts.push_back(t);
	return 0;
}

int gene::clear()
{
	transcripts.clear();
	exons.clear();
	return 0;
}

int gene::add_exon(const exon &e)
{
	exons.push_back(e);
	return 0;
}

int gene::build_transcripts()
{
	transcripts.clear();
	map<string, int> m;
	for(int i = 0; i < exons.size(); i++)
	{
		const exon &e = exons[i];
		if(m.find(e.transcript_id) == m.end())
		{
			transcript t;
			t.add_exon(e);
			m.insert(pair<string, int>(e.transcript_id, transcripts.size()));
			transcripts.push_back(t);
		}
		else
		{
			transcripts[m[e.transcript_id]].add_exon(e);
		}
	}
	return 0;
}

string gene::get_seqname() const
{
	if(exons.size() == 0) return "";
	else return exons[0].seqname;
}

string gene::get_gene_id() const
{
	if(exons.size() == 0) return "";
	else return exons[0].gene_id;
}

int gene::set_gene_id(const string &id) 
{
	for(int i = 0; i < transcripts.size(); i++)
	{
		transcripts[i].gene_id = id;
	}
	for(int i = 0; i < exons.size(); i++)
	{
		exons[i].gene_id = id;
	}
	return 0;
}

int gene::sort()
{
	std::sort(exons.begin(), exons.end());
	for(int i = 0; i < transcripts.size(); i++)
	{
		transcripts[i].sort();
	}
	return 0;
}

set<int32_t> gene::get_exon_boundaries() const
{
	set<int32_t> s;
	for(int i = 0; i < exons.size(); i++)
	{
		int32_t l = exons[i].start;
		int32_t r = exons[i].end;
		s.insert(l);
		s.insert(r);
	}
	return s;
}

PI32 gene::get_bounds() const
{
	if(exons.size() == 0) return PI32(-1, -1);
	int32_t l = exons[0].start;
	int32_t r = exons[exons.size() - 1].end;
	return PI32(l, r);
}

int gene::write(ofstream &fout) const
{
	for(int i = 0; i < transcripts.size(); i++)
	{
		transcripts[i].write(fout);
	}
	return 0;
}