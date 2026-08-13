#!/usr/bin/env bash
#
# One-off bootstrap of the Kii application the large tests run against.
#
# This is an administrative operation against a live application, not part of
# any build, which is why it is a script rather than a CMake target. Run it by
# hand when standing up a new test application:
#
#   APP_ID=... DEFAULT_SITE=... CLIENT_ID=... CLIENT_SECRET=... \
#       tests/large_test/initapp.sh
#
# Requires curl and jq.
#
# Note the app configuration at the end: the "Thing auth with lock/unlock" case
# in thing_test.cpp depends on maxLoginAttempts and a short
# loginLockPeriodSeconds, and fails with no obvious cause if they are absent.
# Lockout counts failed attempts per login name within
# loginAttemptsTimeoutSeconds and a successful authentication resets the count,
# so the identities shared across the suites are not affected by each other.

set -euo pipefail

: "${APP_ID:?APP_ID must be set}"
: "${DEFAULT_SITE:?DEFAULT_SITE must be set}"
: "${CLIENT_ID:?CLIENT_ID must be set}"
: "${CLIENT_SECRET:?CLIENT_SECRET must be set}"

api="https://${DEFAULT_SITE}"
app_hdr=(-H "X-Kii-AppID: ${APP_ID}" -H "X-Kii-AppKey: dummy")

echo "== get app admin token"
ADMIN_TOKEN=$(curl -fsS -X POST "${app_hdr[@]}" \
    -H 'Content-Type: application/json' \
    "${api}/api/oauth2/token" \
    -d "{\"client_id\":\"${CLIENT_ID}\",\"client_secret\":\"${CLIENT_SECRET}\"}" \
    | jq -r .access_token)
if [ -z "${ADMIN_TOKEN}" ] || [ "${ADMIN_TOKEN}" = "null" ]; then
    echo "failed to obtain an admin token" >&2
    exit 1
fi
auth_hdr=(-H "Authorization: Bearer ${ADMIN_TOKEN}")

echo "== create app scope topic"
curl -fsS -X PUT "${app_hdr[@]}" "${auth_hdr[@]}" \
    "${api}/api/apps/${APP_ID}/topics/test_topic"

echo "== deploy server code"
CODE_VERSION=$(curl -fsS -X POST "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/javascript' \
    "${api}/api/apps/${APP_ID}/server-code" \
    -d 'function echo(params, context) { return params.message; }' \
    | jq -r .versionID)
curl -fsS -X PUT "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: text/plain' \
    "${api}/api/apps/${APP_ID}/server-code/versions/current" \
    -d "${CODE_VERSION}"

echo "== create thing type and firmware version"
curl -fsS -X PUT "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/vnd.kii.ThingTypeConfigurationRequest+json' \
    "${api}/api/apps/${APP_ID}/configuration/thing-types/ltest_thing_type" \
    -d '{"verificationCodeFlowStartedByUser":true,"simpleFlow":true,"verificationCodeFlowStartedByThing":true,"verificationCodeTimeout":172800,"verificationCodeLength":9}'
curl -fsS -X PUT "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: text/plain' \
    "${api}/api/apps/${APP_ID}/configuration/thing-types/ltest_thing_type/firmware-versions/ltest_firmware_version"

echo "== create trait and trait alias"
curl -fsS -X POST "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/vnd.kii.TraitCreationRequest+json' \
    "${api}/thing-if/apps/${APP_ID}/traits/ltest_trait/versions" \
    -d '{"dataGroupingInterval":"15_MINUTES","actions":[],"states":[{"string_field":{"description":"","payloadSchema":{"type":"string"}}}]}'
curl -fsS -X PUT "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/vnd.kii.TraitAliasCreationRequest+json' \
    "${api}/thing-if/apps/${APP_ID}/configuration/thing-types/ltest_thing_type/firmware-versions/ltest_firmware_version/aliases/ltest_trait_alias" \
    -d '{"traitVersion":"1","trait":"ltest_trait"}'

echo "== create test user for KHC post test"
curl -fsS -X POST "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/vnd.kii.RegistrationRequest+json' \
    "${api}/api/apps/${APP_ID}/users" \
    -d '{"loginName":"pass-1234","password":"1234","displayName":"test-user"}'

echo "== modify app settings '{\"maxLoginAttempts\":2,\"loginLockPeriodSeconds\":3}'"
curl -fsS -X PATCH "${app_hdr[@]}" "${auth_hdr[@]}" \
    -H 'Content-Type: application/vnd.kii.AppConfigParamsModificationRequest+json' \
    "${api}/api/apps/${APP_ID}/configuration/parameters" \
    -d '{"maxLoginAttempts":2,"loginLockPeriodSeconds":3}'

echo "== done"
