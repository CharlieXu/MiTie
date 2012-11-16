#!/bin/bash


fn_pred="/fml/ag-raetsch/nobackup/projects/rgasp/mgene_predictions/elegans/lsl/RNA_seq_strand_specific/output_cleave_12/genome_wide_predictions/genes_31-Mar-2010.mat /fml/ag-raetsch/nobackup/projects/rgasp/mgene_predictions/elegans/lsl/RNA_seq_label_nc_better_label_filter_reads20_0/output/genome_wide_predictions/genes_07-Jul-2011.mat"


MAT="/fml/ag-raetsch/share/software/matlab-7.6/bin/matlab -nojvm -nodesktop -nosplash"
addpaths="dbstop error; addpath matlab; mip_paths;"

for f in $fn_pred; do
	fn_gtf=${f%.mat}.gtf
	if [ ! -f $fn_gtf ]; then
		echo creating $fn_gtf
		${MAT} -r "$addpaths load('$f', 'genes'); write_gtf(genes, '$fn_gtf', 'mGene'); exit"
	fi
done

fn_bam="/fml/ag-raetsch/nobackup/projects/sequencing_runs/worm-Dec09/reads/elegans/polyA_left_sam_stranded.mapped.2.bam /fml/ag-raetsch/nobackup/projects/sequencing_runs/worm-Dec09/reads/elegans/polyA_right_sam_stranded.mapped.2.bam"


# create segment graph and store in file
########################################
dir=`dirname $0`
for f in $fn_pred; do
	out_dir=`dirname $f`/mitie
	fn_graph=${out_dir}/graphs.bin
	echo $fn_graph
	fn_gtf=${f%.mat}.gtf
	mkdir -p $out_dir
	opts="--seg-filter 0.05 --region-filter 100 --tss-tts-pval 0.0001"

	#rm $fn_graph

	if [ ! -f $fn_graph ]
	then
		$dir/generate_segment_graph ${fn_graph}.tmp $opts --gtf $fn_gtf $fn_bam
		mv ${fn_graph}.tmp $fn_graph
	fi
done

for f in $fn_pred; do
	out_dir=`dirname $f`/mitie
	mip_dir=${out_dir}/pred/
	mkdir -p $mip_dir
	fn_graph=${out_dir}/graphs.bin
	${MAT} -r "$addpaths denovo('$fn_graph', {'`echo $fn_bam | sed "s/ /','/g"`'}, '$mip_dir'); exit"
done
