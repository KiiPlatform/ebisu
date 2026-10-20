#!/usr/bin/env bash
#
# Cleans up after the large tests in the Kii application they run against.
# It deletes the things and users the tests create, by the names the tests give
# them. The thing and the user that tests use by name, test1 and pass-1234, are
# kept, but every token of theirs is revoked.
#
# This is an administrative operation against a live application, like
# initapp.sh. The cleanapp workflow runs it with the credentials, or run it by
# hand:
#
#   APP_ID=... DEFAULT_SITE=... CLIENT_ID=... CLIENT_SECRET=... \
#       tests/large_test/cleanapp.sh [--delete]
#
# Without --delete it only reports what it would do. Anything else in the
# application is left alone and listed by its vendor thing ID or login name.
#
# Nothing it prints holds a token or a password: responses are parsed, never
# printed, and in GitHub Actions each admin token is masked as soon as it is
# obtained. Each admin token lasts ten minutes, a new one replaces it before it
# expires, and the last is revoked at the end. Do not run it with `bash -x`,
# which would print them.
#
# Requires curl and jq.

set -euo pipefail

: "${APP_ID:?APP_ID must be set}"
: "${DEFAULT_SITE:?DEFAULT_SITE must be set}"
: "${CLIENT_ID:?CLIENT_ID must be set}"
: "${CLIENT_SECRET:?CLIENT_SECRET must be set}"

delete=false
if [ "${1:-}" = "--delete" ]; then
    delete=true
fi

api="https://${DEFAULT_SITE}"
app="${api}/api/apps/${APP_ID}"
app_hdr=(-H "X-Kii-AppID: ${APP_ID}" -H "X-Kii-AppKey: dummy")

# Vendor thing IDs and login names the tests create.
created_things='^(thing2-|thing3-|LTest-|ltest_|ltest-)'
created_users='^ltest_'
# Used by name, so kept.
shared_thing='test1'
shared_user='pass-1234'

work=$(mktemp -d)
token=""
token_at=0

revoke() {
    curl -sS -o /dev/null -X POST "${app_hdr[@]}" \
        -H "Authorization: Bearer $1" \
        -H 'Content-Type: application/vnd.kii.LogoutRequest+json' \
        "${app}/logout" -d "{\"type\":\"SPECIFIC_TOKEN\",\"token\":\"$1\"}" || true
}

cleanup() {
    if [ -n "${token}" ]; then
        revoke "${token}"
    fi
    rm -rf "${work}"
}
trap cleanup EXIT

# An admin token, a new one when the one held is over eight minutes old.
admin_token() {
    local now
    now=$(date +%s)
    if [ -n "${token}" ] && [ $((now - token_at)) -lt 480 ]; then
        return
    fi
    local expires=$(( (now + 600) * 1000 ))
    local fresh
    fresh=$(curl -sS -X POST "${app_hdr[@]}" -H 'Content-Type: application/json' \
        "${api}/api/oauth2/token" \
        -d "{\"client_id\":\"${CLIENT_ID}\",\"client_secret\":\"${CLIENT_SECRET}\",\"expiresAt\":${expires}}" \
        | jq -r '.access_token // empty' 2>/dev/null) || fresh=""
    if [ -z "${fresh}" ]; then
        echo "failed to obtain an admin token" >&2
        exit 1
    fi
    if [ -n "${GITHUB_ACTIONS:-}" ]; then
        echo "::add-mask::${fresh}"
    fi
    if [ -n "${token}" ]; then
        revoke "${token}"
    fi
    token="${fresh}"
    token_at=${now}
}

# call METHOD PATH [CONTENT-TYPE BODY]: the response body goes to ${work}/resp,
# the status to ${status}. A 429 is retried.
call() {
    local attempt
    for attempt in 1 2 3 4 5; do
        admin_token
        local args=(-sS -o "${work}/resp" -w '%{http_code}' -X "$1" "${app_hdr[@]}"
            -H "Authorization: Bearer ${token}")
        if [ -n "${3:-}" ]; then
            args+=(-H "Content-Type: $3" --data-binary "$4")
        fi
        status=$(curl "${args[@]}" "${app}$2") || status=000
        if [ "${status}" != 429 ]; then
            return
        fi
        sleep "${attempt}"
    done
}

# query PATH CONTENT-TYPE QUERY-FIELD JQ-FILTER OUTPUT: every result of an
# admin query, one line each.
query() {
    local key="" body
    : > "$5"
    while :; do
        body="{\"$3\":{\"clause\":{\"type\":\"all\"}},\"bestEffortLimit\":200"
        if [ -n "${key}" ]; then
            body+=",\"paginationKey\":\"${key}\""
        fi
        body+="}"
        call POST "$1" "$2" "${body}"
        if [ "${status}" != 200 ]; then
            echo "$1: status ${status}" >&2
            exit 1
        fi
        jq -r "$4" "${work}/resp" >> "$5"
        key=$(jq -r '.nextPaginationKey // empty' "${work}/resp")
        if [ -z "${key}" ]; then
            break
        fi
    done
}

# delete_each PATH-PREFIX LIST: deletes every ID in the first column.
delete_each() {
    local n=0 failed=0 id
    while IFS=$'\t' read -r id _; do
        call DELETE "$1/${id}"
        if [ "${status}" = 204 ] || [ "${status}" = 404 ]; then
            n=$((n + 1))
        else
            failed=$((failed + 1))
        fi
        if [ $(( (n + failed) % 200 )) = 0 ]; then
            echo "  ${n} deleted, ${failed} failed"
        fi
    done < "$2"
    echo "  ${n} deleted, ${failed} failed"
}

# logout_all PATH: revokes every token of the principal.
logout_all() {
    call POST "$1/logout" application/vnd.kii.LogoutRequest+json '{"type":"ALL_TOKENS"}'
    echo "  tokens revoked: status ${status}"
}

echo "== things"
query /things/query application/vnd.kii.ThingQueryRequest+json thingQuery \
    '.results[] | "\(._thingID)\t\(._vendorThingID // "")"' "${work}/things"
awk -F'\t' -v re="${created_things}" '$2 ~ re' "${work}/things" > "${work}/things.created"
awk -F'\t' -v id="${shared_thing}" '$2 == id' "${work}/things" > "${work}/things.shared"
awk -F'\t' -v re="${created_things}" -v id="${shared_thing}" '$2 !~ re && $2 != id' \
    "${work}/things" > "${work}/things.other"
echo "  $(wc -l < "${work}/things") in all: $(wc -l < "${work}/things.created") created by the tests," \
    "$(wc -l < "${work}/things.shared") ${shared_thing}, $(wc -l < "${work}/things.other") other"
if [ -s "${work}/things.other" ]; then
    echo "  other things, kept (vendor thing IDs):"
    cut -f2 "${work}/things.other" | sort | sed 's/^/    /'
fi

echo "== users"
query /users/query application/vnd.kii.UserQueryRequest+json userQuery \
    '.results[] | "\(.userID)\t\(.loginName // "")"' "${work}/users"
awk -F'\t' -v re="${created_users}" '$2 ~ re' "${work}/users" > "${work}/users.created"
awk -F'\t' -v id="${shared_user}" '$2 == id' "${work}/users" > "${work}/users.shared"
awk -F'\t' -v re="${created_users}" -v id="${shared_user}" '$2 !~ re && $2 != id' \
    "${work}/users" > "${work}/users.other"
echo "  $(wc -l < "${work}/users") in all: $(wc -l < "${work}/users.created") created by the tests," \
    "$(wc -l < "${work}/users.shared") ${shared_user}, $(wc -l < "${work}/users.other") other"
if [ -s "${work}/users.other" ]; then
    echo "  other users, kept (login names):"
    cut -f2 "${work}/users.other" | sort | sed 's/^/    /'
fi

if [ "${delete}" != true ]; then
    echo "== dry run: nothing deleted or revoked; run with --delete to do it"
    exit 0
fi

echo "== deleting the things the tests created"
delete_each /things "${work}/things.created"
echo "== deleting the users the tests created"
delete_each /users "${work}/users.created"
while IFS=$'\t' read -r id _; do
    echo "== ${shared_thing}"
    logout_all "/things/${id}"
done < "${work}/things.shared"
while IFS=$'\t' read -r id _; do
    echo "== ${shared_user}"
    logout_all "/users/${id}"
done < "${work}/users.shared"
echo "== done"
