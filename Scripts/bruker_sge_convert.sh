#!/bin/bash -eu
USAGE="Usage: $0 [options] input_directory output_directory

Converts an entire Bruker in parallel using a Sun
Grid Engine queue system.

Options (must go first):
    -e EXT   : Use a different extension, e.g. .nrrd
    -l       : Keep localizers
    -q QUEUE : Use QUEUE
    -z       : Create .nii.gz files instead of .nii files
"

LOCALIZERS=""
EXT=".nii"
QUEUE=""
while getopts "lqz" opt; do
    case $opt in
        e) EXT="$OPTARG";;
        l) LOCALIZERS="1";;
        q) QUEUE="-q $OPTARG";;
        z) EXT=".nii.gz";;
    esac
done
shift  $((OPTIND - 1))
if [ -z "${1-}" ]; then
    printf "$USAGE"
    exit 1
fi
SRC_DIR="$1"
TGT_DIR="$2"
LOG_DIR="$PWD/convert_logs"
mkdir -p $LOG_DIR
mkdir -p $TGT_DIR

for IMG in $( ls "$SRC_DIR"/[0-9]*/pdata/*/2dseq | sort -n ); do
    ## Interrupted scans may end up with a 2dseq file with 0 bytes, annoyingly
    ## Or no visu_pars file
    IMG_DIR=$( dirname $IMG )
    VISU_PARS="$IMG_DIR/visu_pars"
    if [[ -s $IMG && -f $VISU_PARS ]]; then
        PROTOCOL=$( grep -A1 "\#\#\$VisuAcquisitionProtocol" $VISU_PARS | tail -n1 )
        if [[ -z "$LOCALIZERS" && $PROTOCOL == *"Localizer"* ]]; then
            echo "$IMG is a localizer, skipping"
        else
            echo "Queueing $IMG"
            qsub -o ${LOG_DIR}/ -e ${LOG_DIR}/ -j y $QUEUE ${SCRIPT} "$IMG $EXT $TGT_DIR"
        fi
    else
        echo "$IMG is empty, skipping."
    fi
done
