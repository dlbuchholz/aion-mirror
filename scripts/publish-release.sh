#!/usr/bin/env bash
# Publishes a build on Codeberg (Forgejo API).
#
#   scripts/publish-release.sh <file>...
#
# Run by CI after a successful push to develop, with these set (CI provides the first three):
#   GITHUB_SERVER_URL, GITHUB_REPOSITORY, GITHUB_SHA   where and which commit
#   RELEASE_TOKEN                                      a token allowed to create tags and releases
#
# 1. The rolling pre-release "develop" is replaced: always the newest build, at the same address.
# 2. If the version in CMakeLists.txt has no tag yet, the tag v<version> is created on this commit
#    with a draft release "Aion <version>". Add notes on Codeberg and publish it from there.
set -euo pipefail

rolling_tag=develop
api="$GITHUB_SERVER_URL/api/v1/repos/$GITHUB_REPOSITORY"
source_dir=$(realpath "$(dirname "$0")/..")
version=$(sed -n 's/^project(Aion VERSION \([0-9.]*\).*/\1/p' "$source_dir/CMakeLists.txt")
short_sha=${GITHUB_SHA:0:7}
files=("$@")

# curl against the API. Prints the response body and fails on errors, except the HTTP codes
# passed in ALLOW (like 404 for "doesn't exist").
call() {
    local method=$1 path=$2 status body
    shift 2
    body=$(mktemp)
    status=$(curl -sS -o "$body" -w '%{http_code}' -X "$method" \
             -H "Authorization: token $RELEASE_TOKEN" "$@" "$api$path")
    if [[ $status != 2* && " ${ALLOW:-} " != *" $status "* ]]; then
        echo "$method $path failed with HTTP $status: $(cat "$body")" >&2
        rm -f "$body"
        return 1
    fi
    cat "$body"
    rm -f "$body"
}

# The first "id" in a JSON response (a release's own id comes first)
id_of() { sed -n 's/^{"id":\([0-9]*\).*/\1/p'; }

# Creates a release for an existing tag and uploads the files to it
create_release() {
    local json=$1 release_id file
    release_id=$(call POST /releases -H 'Content-Type: application/json' -d "$json" | id_of)
    for file in "${files[@]}"; do
        call POST "/releases/$release_id/assets?name=$(basename "$file")" \
             -F "attachment=@$file" > /dev/null
    done
}

# 1. Rolling pre-release: remove the old one and its tag, create both anew on this commit
old_id=$(ALLOW=404 call GET "/releases/tags/$rolling_tag" | id_of)
if [[ -n $old_id ]]; then
    call DELETE "/releases/$old_id" > /dev/null
fi
ALLOW=404 call DELETE "/tags/$rolling_tag" > /dev/null
call POST /tags -H 'Content-Type: application/json' \
     -d "{\"tag_name\":\"$rolling_tag\",\"target\":\"$GITHUB_SHA\"}" > /dev/null
create_release "{\"tag_name\":\"$rolling_tag\",\"name\":\"Development build\",\"prerelease\":true,
\"body\":\"The newest build of the develop branch, from commit $short_sha ($(date -u +%Y-%m-%d)). For trying out new things: it can have bugs, so don't use it with your only copy of real data.\"}"
echo "Updated the $rolling_tag pre-release"

# 2. A new version: tag it and prepare a draft release
existing=$(ALLOW=404 call GET "/tags/v$version")
if [[ $existing == *'"name"'* ]]; then
    echo "v$version is already tagged"
else
    call POST /tags -H 'Content-Type: application/json' \
         -d "{\"tag_name\":\"v$version\",\"target\":\"$GITHUB_SHA\",\"message\":\"Aion $version\"}" > /dev/null
    create_release "{\"tag_name\":\"v$version\",\"name\":\"Aion $version\",\"draft\":true,
\"body\":\"Release notes go here.\"}"
    echo "Tagged v$version and created a draft release; publish it on Codeberg"
fi
