#!/usr/bin/env bash
# set -e

BUILD_DIR=$1
TAR_NAME=$2
AFI_NAME=$3

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Upload design to S3
aws s3 cp $BUILD_DIR/$TAR_NAME s3://xbili-fyp/fma/
touch LOG_FILES_GO_HERE.txt
aws s3 cp LOG_FILES_GO_HERE.txt s3://xbili-fyp/fma_logs/

# Start AFI creation
aws ec2 create-fpga-image \
--region us-east-1 \
--name $AFI_NAME \
--description "Created by xbili" \
--input-storage-location Bucket="xbili-fyp/fma",Key=$TAR_NAME \
--logs-storage-location Bucket="xbili-fyp/fma_logs",Key=LOG_FILES_GO_HERE.txt

echo "Success: Deployment of FPGA image to AWS."
echo "Run aws ec2 describe-fpga-images --fpga-image-ids afi-<afi-id> to check for upload status."

cd $DIR
