#include <assert.h>
#include <string>
	using std::string;
#include <string.h>
#include <stdio.h>
#include <vector>
	using std::vector;
#include <map>
	using std::map;
#include "gtf_tools.h"
#include <algorithm>
#include "region.h"
#include "tools.h"

char* get_attribute(char* line, const char* tag)
{
	int len=strlen(tag);
	char* pos = strstr(line, tag);
	if (pos==NULL)
		return NULL;

	char* start = pos+len;

	while (*start!='"' && *start !=';' && start<line+strlen(line))
	{
		start++;
	}
	start++;
	char* result = new char[101];
	
	for (int i=0; i<100; i++)
	{
		if (start+i>=line+strlen(line))
		{
			printf("%p, %p\n", start+i, line+strlen(line));
			break;
		}

		if (start[i]=='"' || start[i]==';')
			break;

		result[i] = start[i];
		result[i+1] = '\0';
	}
	//printf("\n");
	return result;
}

vector<char*> get_fields(char* line)
{
	vector<char*> ret;
	int i=0;
	ret.push_back(line);
	while (line[i]!=0)
	{
		if (line[i]=='\t')
		{
			line[i]=0;
			ret.push_back(line+i+1);
		}
		i++;
	}
	return ret;
}

vector<Region*> parse_gtf(char* gtf_file)
{
	
	FILE* fd = fopen(gtf_file, "r");
	if (!fd)
	{
		printf("Could not open file: %s\n", gtf_file);
		exit(-1);
	}
	int cnt = 0;
	map<string, Region*> transcripts;
	while (~feof(fd))
	{
		char line[1000];
		if (fgets(line, 1000, fd)==NULL) break;

		if (++cnt%1000==0)
			printf("\rreading line %i (%i transcripts)", cnt, (int) transcripts.size());

		if (line[0] == '#') // disregard comments
			continue;

		vector<char*> fields = get_fields(line);

		//for (int i=0; i<fields.size(); i++)
		//	printf("%s\n", fields[i]);	

		if (fields.size()!=9)
		{
			printf("wrong number of fields (%i): skip over line\n", (int) fields.size());
			for (int i=0; i<(int)fields.size(); i++)
			{
				printf("%s\t", fields[i]);
			}
			printf("\n");
			continue;
		}

		char* type = fields[2];
		char strand = fields[6][0];
		char* chr = fields[0];

		// parse exons
		if (strcmp(type, "exon")==0)
		{
			char* tr_id = get_attribute(fields[8], "transcript_id");
			if (!tr_id)
			{
				printf("Could not find transcript_id: %s\n", fields[8]);
				exit(-1);
			}
			string transcript_id(tr_id);
			delete[] tr_id;

			int start = atoi(fields[3]);
			int stop = atoi(fields[4]);
			//printf("exons: %i->%i\n", start, stop);	
			segment* seg = new segment;
			seg->first = start;
			seg->second = stop;
			Region* reg = transcripts[transcript_id];
			if (!reg)
			{
				reg = new Region(start, stop, chr, strand);
				transcripts[transcript_id] = reg;
				vector<segment> vec;
				reg->transcripts.push_back(vec);
			}
			reg->transcripts[0].push_back(*seg);
			delete seg;
		}
		//else
		//{
		//	printf("skip over line: %s\n", line);
		//}
	}
	
	vector<Region*> regions;
	map<string, Region*>::iterator it;
	for (it=transcripts.begin(); it!=transcripts.end(); it++)
	{
		Region* reg = it->second;
		string transcript_id = it->first;
		
		reg->transcript_names.push_back(transcript_id);

		// sort exons
		sort(reg->transcripts[0].begin(), reg->transcripts[0].end(), compare_second);

		// addjust start and stop
		reg->start = reg->transcripts[0].front().first;
		reg->stop = reg->transcripts[0].back().second;

		regions.push_back(reg);
	}

	// merge overlapping transcripts into genes
	bool change = true;
	int iter = 0;
	printf("\n");
	while (change)
	{
		printf("parse_gtf: merge iteration: %i size:%i\n", iter++, (int) regions.size());
		if (iter>10)
			break;
		change = false;	

		vector<vector<int> > ov_list = region_overlap(regions, regions);
		for (uint i=0; i<regions.size(); i++)
		{
			for (uint j=0; j<ov_list[i].size(); j++)
			{
				// self overlap
				if (ov_list[i][j]==(int) i)
					continue;
				if (strcmp(regions[i]->chr, regions[ov_list[i][j]]->chr)!=0)
					continue;
				if (regions[i]->strand != regions[ov_list[i][j]]->strand)
					continue;
				if (regions[ov_list[i][j]]->start==-1 || regions[i]->start==-1)
					continue;

				Region* r1 = regions[i];
				Region* r2 = regions[ov_list[i][j]];
				change = true;
				// append transcripts of r2 to r1
				r1->transcripts.insert(r1->transcripts.end(), r2->transcripts.begin(), r2->transcripts.end()); 
				r1->transcript_names.insert(r1->transcript_names.end(), r2->transcript_names.begin(), r2->transcript_names.end()); 

				r1->start = std::min(r1->start, r2->start);
				r1->stop = std::max(r1->stop, r2->stop);
				r2->start = -1;
			}
		}
		// remove merged regions
		vector<Region*> tmp;
		for (uint i=0; i<regions.size(); i++)
			if (regions[i]->start>-1)
				tmp.push_back(regions[i]);
			else
				delete regions[i];

		regions = tmp;
	}
	return regions;
}
