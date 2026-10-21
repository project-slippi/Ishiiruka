#!/bin/zsh
#
# Signing and notarizing only happens on builds where the CI has access
# to the necessary secrets; this avoids builds in forks where secrets
# shouldn't be.
#
# Portions of the notarization response checks are borrowed from:
#
# https://github.com/smittytone/scripts/blob/main/packcli.zsh
#
# (They've done the work of figuring out what the reponse formats are, etc)
#
# Apple's notary service occasionally times out mid-request, so each call to it
# is retried instead of failing a build that took a long time to produce.

version="$(echo $GIT_TAG)"
identifier="com.project-slippi.dolphin"
filepath=${1:?"need a filepath"}

# Notarization normally finishes in under a minute. Each step below retries
# until this long after the upload started; if it still hasn't worked by then,
# something is actually wrong.
max_wait_secs=600
retry_delay_secs=15

auth_args=(
    --issuer ${APPLE_ISSUER_ID}
    --key-id ${APPLE_API_KEY}
    --key ~/private_keys/AuthKey_${APPLE_API_KEY}.p8
)

# Prints a field from notarytool's output, e.g. `field status "${response}"`
# for a line like "  status: Accepted".
field() {
    grep -m 1 "^ *$1: " <<< "$2" | sed "s/^ *$1: //"
}

# Succeeds if there's still time to wait and try again before the deadline.
time_for_retry() {
    (( $(date +%s) + retry_delay_secs < deadline ))
}

echo "Attempting notarization"
s_time=$(date +%s)
deadline=$((s_time + max_wait_secs))

# Submit the DMG for notarization. A submission whose upload failed never gets
# processed, so a failed attempt starts over.
job_id=""
while [[ -z ${job_id} ]]; do
    submit_response=$(xcrun notarytool submit "${filepath}" "${auth_args[@]}" 2>&1)
    if [[ $? -eq 0 ]]; then
        job_id=$(field id "${submit_response}")
    fi

    if [[ -z ${job_id} ]]; then
        echo "${submit_response}"
        if ! time_for_retry; then
            echo "Could not submit ${filepath} for notarization"
            exit 1
        fi
        echo "Submission failed, retrying in ${retry_delay_secs} seconds"
        sleep ${retry_delay_secs}
    fi
done
echo "Submitted for notarization. Job ID: ${job_id}"

# Poll until Apple finishes processing the submission. A failed status request
# just means we don't have an answer yet.
status_result="In Progress"
while [[ ${status_result} == "In Progress" ]]; do
    sleep ${retry_delay_secs}
    info_response=$(xcrun notarytool info "${job_id}" "${auth_args[@]}" 2>&1)
    if [[ $? -eq 0 ]]; then
        status_result=$(field status "${info_response}")
        echo "Status: ${status_result}"
    else
        echo "Status check failed, retrying: ${info_response}"
    fi

    if [[ ${status_result} == "In Progress" ]] && ! time_for_retry; then
        echo "Gave up waiting on job ${job_id} after ${max_wait_secs} seconds"
        exit 1
    fi
done

# Log some debug timing info.
e_time=$(date +%s)
n_time=$((e_time - s_time))
echo "Notarization call completed after ${n_time} seconds. Job ID: ${job_id}"

# Fetch and echo the log *before* bailing if it's bad, so we can tell if there's
# a deeper error we need to handle. It's only informational, so failing to fetch
# it doesn't fail the build.
while true; do
    log_response=$(xcrun notarytool log "${auth_args[@]}" "${job_id}" 2>&1) && break
    time_for_retry || break
    sleep ${retry_delay_secs}
done
echo "${log_response}"

if [[ ${status_result} != "Accepted" ]]; then
    echo "Notarization failed with status ${status_result}"
    exit 1
fi

# Attempt to staple the notarization result to the app. The ticket can take a
# moment to become available after acceptance, so this is retried too.
echo "Successfully notarized! Stapling notarization status to ${filepath}"
until xcrun stapler staple "${filepath}"; do
    if ! time_for_retry; then
        echo "Could not staple notarization to app"
        exit 1
    fi
    echo "Stapling failed, retrying in ${retry_delay_secs} seconds"
    sleep ${retry_delay_secs}
done

# Confirm the staple actually worked...
echo "Checking notarization to ${filepath}"
spctl --assess -vvv --type install "${filepath}"
