#!/bin/bash



# Fail on any error by default
set -e

###############################################################################
# EXECUTION
###############################################################################

# Consume the first argument, or default to 'usage' as command
RAWCOMMAND="usage"
if [ "$#" -ge 1 ]; then
    COMMAND=$1
    shift
fi
readonly COMMAND
readonly ARGS=($@)

readonly WORKSPACE="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"

echo "
-----------------------------------------------------
 * workspace: ${WORKSPACE}
 * command:   ${COMMAND}
 * arguments: ${ARGS[*]}
-----------------------------------------------------
"

function_exists() {
    declare -f -F $1 > /dev/null
    return $?
}

usage() {
  echo "Usage: ./make.sh <command> [arguments]

    Commands:
    usage:               Show this usage information.
    generateSecret:      Generate / Re-generate shared secret between device and cloud services
    deploy:              Deploys project to Google App Engine. Arguments: projectid:
"
}

generateSecret() {
    tempFile="secret-key.txt"
    echo
    echo "* writing new secret to temp file ${tempFile}"
    openssl rand -hex 60 > ${tempFile}
}

deploy() {
    local readonly project_id=${1}
    if [[ "${project_id}" == "" ]]; then
        echo "ERROR: project_id must be supplied as argument"
        return 1
    fi

    echo "* Deploying auth token ${VERSION} to ${project_id}"

    echo
    echo "* Ensuring keyring ${KEYRING} exists globally in project ${project_id}"

     if [[ ! $(gcloud kms keyrings list --location=global --project=${project_id} | grep "${KEYRING}") ]]; then
        echo "CREATING KEY RING ${KEYRING}"
        cloud kms keyrings create ${KEYRING} --location=global --project=${project_id}
     else
        echo "KEY RING ${KEYRING} ALREADY EXISTS, NOT CREATING"
     fi

      echo
      echo "* Ensuring key ${KEY} exists on key ring ${KEYRING} in project ${project_id}"
     if [[ ! $(gcloud kms keys list --location=global  --keyring=${KEYRING} --project=${project_id} | grep "${KEY}") ]]; then
        echo "CREATING KEY ${KEY}"
        gcloud kms keys create service-2-service \
                              --location=global \
                              --keyring=${KEYRING} \
                              --purpose encryption \
                              --project=${project_id}
     else
        echo "KEY ${KEY} ON KEY RING ${KEYRING} ALREADY EXISTS, NOT CREATING"
     fi

    echo
    echo "* Ensuring authentication bucket ${authBucket} exists in project ${project_id}"
    #creates bucket if necessary
    if [[ ! $(gsutil ls -p ${project_id} | grep "${authBucket}") ]]; then
        echo "CREATING BUCKET ${authBucket}"
        gsutil mb -p ${project_id} -c regional -l us-central1 gs://${authBucket}
    else
        echo "BUCKET ${authBucket} ALREADY EXISTS, NOT CREATING"
    fi

    echo
    echo "* checking if secret version ${VERSION} already exists in ${authBucket}"
        secretFileName="${VERSION}.key"
        secretFile="/tmp/${secretFileName}"
        gsKeyFileName=${KEYRING}/${KEY}/${secretFileName}
        if [[ $(gsutil ls gs://${authBucket}/${gsKeyFileName} | grep ${gsKeyFileName}) ]]; then
        echo "SECRET VERSION ${VERSION} ALREADY EXISTS"
        echo "secrets can't be overwritten, change VERSION to create new secret."
        return 0
    fi

    tempFile="/tmp/${KEYRING}-${KEY}-${VERSION}.tmp"
    echo
    echo "* writing new secret to temp file ${tempFile}"
    openssl rand -base64 60 >> ${tempFile}

    echo
    echo "* encrypting ${tempFile} to ${secretFileName}"
    gcloud kms encrypt --ciphertext-file=${secretFile} \
                       --plaintext-file=${tempFile} \
                       --key=${KEY} --keyring=${KEYRING} \
                       --location=global --project=${project_id}


    echo
    echo "* copying ${secretFile} to ${authBucket}"
    gsutil cp ${secretFile} gs://${authBucket}/${gsKeyFileName}

    echo
    echo "* deleting temp files ${tempFile}, ${secretFile}"
    rm ${tempFile}
    rm ${secretFile}

    echo
    echo "* DONE, SECRET ${VERSION} HAS BEEN SUCCESSFULLY GENERATED AND SAVED IN gs://${authBucket}/${gsKeyFileName}"

#5. encrypt file
#gcloud kms encrypt --ciphertext-file=/tmp/service-2-service-v1.txt \
#                   --plaintext-file=/tmp/key-v1.txt \
#	               --key=service-2-service --keyring=pmitc-secret \
#	               --location=global --project=pc-pmitc
#
#6. put file in bucket and set ACL to read only
#
#7. delete files

}


if ! function_exists ${COMMAND}; then
    echo "ERROR: Unknown command '${COMMAND}'"
    usage
    exit 1
elif [[ "${COMMAND}" == "usage" ]]; then
    usage
    exit 0
elif [[ "${COMMAND}" == "generateSecret" ]]; then
    ${COMMAND} ${ARGS[*]}
    exit 0
fi
elif [[ "${COMMAND}" == "deploy" ]]; then
    ${COMMAND} ${ARGS[*]}
    exit 0
fi



