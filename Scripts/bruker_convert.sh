#!/bin/bash -eu
USAGE="Usage: $0 [options] input_directory output_directory

Converts an entire Bruker dataset into Nifti format. Will generate diffusion
.bvec and .bval files for all DTI acquisitions.

Options (must go first):
    -l : Keep localizers
    -z : Create .nii.gz files instead of .nii files
"

LOCALIZERS=""
EXT=".nii"
while getopts "lz" opt; do
    case $opt in
        l) LOCALIZERS="1";;
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
            echo "Converting $IMG"
            ../build/nan_bruker $IMG $EXT --prefix="$TGT_DIR"/\
                --rename="VisuExperimentNumber" \
                --rename="VisuProcessingNumber" \
                --rename="VisuAcquisitionProtocol" \
                --rename="VisuSeriesComment"
        fi
    else
        echo "$IMG is empty, skipping."
    fi
done
