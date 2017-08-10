# Overview
Scallop is an accurate reference-based transcript assembler. Scallop features
its high accuracy in assembling multi-exon transcripts as well as lowly
expressed transcripts. Scallop achieves this improvement through a novel
algorithm that can be proved preserving all phasing paths from reads and paired-end reads,
while also achieves both transcripts parsimony and coverage deviation minimization.

**Pre-print** of Scallop is available at [bioRxiv](http://biorxiv.org/content/early/2017/04/03/123612).
The datasets and scripts used in this paper to compare the performance of Scallop
and other assemblers are available at [**scalloptest**](https://github.com/Kingsford-Group/scalloptest).

Please also checkout the **podcast** about Scallop (thanks [Roman Cheplyaka](https://ro-che.info/) for the interview).
It is available at both [the bioinformatics chat](https://bioinformatics.chat/scallop) and
[iTunes](https://itunes.apple.com/us/podcast/the-bioinformatics-chat/id1227281398). 

# Release
Latest release, including both binary and source code, is [here](https://github.com/Kingsford-Group/scallop/releases/tag/v0.10.1).

# Installation
To install Scallop from the source code, you need to first download/compile 
Boost, htslib and Clp, setup the corresponding environmental variables,
and then compile the source code of Scallop.

## Install Boost
Download Boost [(license)](http://www.boost.org/LICENSE_1_0.txt)
from (http://www.boost.org).
Uncompress it somewhere (compiling and installing are not necessary).
Set environment variable `BOOST_HOME` to indicate the directory of Boost.
For example, for Unix platforms, do the following:
```
export BOOST_HOME="/directory/to/your/boost/boost_1_60_0"
```


## Install htslib
Download htslib [(license)](https://github.com/samtools/htslib/blob/develop/LICENSE)
from (http://www.htslib.org/) with version 1.5 or higher.
Choose a directory for installation and set the environment variable `HTSLIB` for that.
For example, for Unix platforms, do the following:
```
export HTSLIB="/directory/to/your/htslib/install"
```
Use the following to build `libhts.a`:
```
autoheader
autoconf
./configure --disable-bz2 --disable-lzma --disable-gcs --disable-s3 --enable-libcurl=no --prefix=$HTSLIB
make
make install
```


## Install Clp
Download Clp [(license)](https://opensource.org/licenses/eclipse-1.0)
from (https://projects.coin-or.org/Clp). 
Choose a directory for installation and set the environment variable `CLP_HOME` for that.
For example, for Unix platforms, do the following:
```
export CLP_HOME="/directory/to/your/clp/install"
```
Use the following to build libraries:
```
./configure --enable-static --disable-bzlib --disable-zlib --prefix=$CLP_HOME
make
make install
```


## Compile Scallop
The compilation of `scallop` requires `automake` and `autoconf` packages.
If they have not been installed, on linux platform, do the following:
```
sudo apt-get install autoconf
sudo apt-get install automake
```

The installation also requires other libraries, `libz, libblas, liblapack`, 
which are dependencies of `libhts` and
`libClp`. Install them if you encounter errors when compiling.

After that run the script `build.sh`, which will generate the executable file `src/src/scallop`.


# Usage

The usage of `scallop` is:
```
./scallop -i <input.bam> -o <output.gtf> [options]
```

The `input.bam` is the read alignment file generated by some RNA-seq aligner, (for example, TopHat2, STAR, or HISAT2).
Make sure that it is sorted; otherwise run `samtools` to sort it:
```
samtools sort input.bam > input.sort.bam
```

The reconstructed transcripts shall be written as gtf format into `output.gtf`.

Scallop support the following parameters. Please also refer
to the additional explanation below the table.

 Parameters | Default Value | Description
 ------------------------- | ------------- | ----------
 --help  |  | print usage of Scallop and exit
 --version | | print version of Scallop and exit
 --preview | | show the inferred `library_type` by sampling reads
 --verbose | 1 | chosen from {0, 1, 2}; 0: quiet; 1: one line for each splice graph; 2: details of graph decomposition
 --library_type               | empty | chosen from {empty, unstranded, first, second}
 --min_transcript_coverage    | 1 | the minimum coverage required to output a multi-exon transcript
 --min_single_exon_coverage   | 20 | the minimum coverage required to output a single-exon transcript
 --min_transcript_length_base      |150 | the minimum base length of a transcript
 --min_transcript_length_increase  | 50 | the minimum increased length of a transcript with each additional exon
 --min_mapping_quality        | 1 | ignore reads with mapping quality less than this value
 --min_bundle_gap             | 50 | the minimum distances required to start a new bundle
 --min_num_hits_in_bundle     | 20 | the minimum number of reads required in a bundle
 --min_flank_length           | 3 | the minimum match length required in each side for a spliced read
 --min_splice_bundary_hits    | 1 | the minimum number of spliced reads required to support a junction

1. `--library_type` is highly recommended to provide. The `unstranded`, `first`, and `second`
correspond to `fr-unstranded`, `fr-firststrand`, and `fr-secondstrand` used in standard Illumina
sequencing libraries. If none of them is given, i.e., it is `empty` by default, then `scallop`
will try to infer the `library_type` by itself (see `--preview`). Notice that such inference is based
on the `XS` tag stored in the input `bam` file. If the input `bam` file do not contain `XS` tag,
then it is essential to provide the `library_type` to `scallop`. You can try `--preview` to see
the inferred `library_type` by `scallop`.

2. `--min_transcript_coverage` is used to filter lowly expressed transcripts: `scallop` will filter
out transcripts whose (predicted) raw counts (number of moleculars) is less than this number.

3. `--min_transcript_length_base` and `--min_transcript_length_increase` is combined to filter
short transcripts: the minimum length of a transcript is given by `--min_transcript_length_base`
+ `--min_transcript_length_increase` * num-of-exons-in-this-transcript. Transcripts that are less
than this number will be filtered out.
