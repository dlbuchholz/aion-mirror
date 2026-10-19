#!/usr/bin/env bash
# Publishes a build on Codeberg (Forgejo API).
#
#   scripts/publish-release.sh <file>...
#
# Run by CI after a successful push to develop, with these set (CI provides the first three):
#   GITHUB_SERVER_URL, GITHUB_REPOSITORY, GITHUB_SHA   where and which commit
#   RELEASE_TOKEN                                      a token allowed to create tags and releases
#
# 1. The rolling pre-release "staging" is replaced: always the newest build, at the same address.
# 2. The version in CMakeLists.txt gets a draft release "Aion <version>" with its tag. Until you
#    publish it on Codeberg, every push moves the tag to the new commit and replaces the files,
#    keeping the notes written in the draft. Once published, it's left alone.
#
# JSON is read and written with Node (the build image has it for the CI actions).
set -euo pipefail

rolling_tag=staging                      # Not "develop": a tag with a branch's name makes pushes ambiguous
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

post_json() { call POST "$1" -H 'Content-Type: application/json' -d "$2"; }

# Runs a JavaScript expression on the JSON from stdin (as "json"); prints the result
json() {
    node -e 'let input = ""; process.stdin.on("data", c => input += c).on("end", () => {
        const json = JSON.parse(input || "null"), args = process.argv.slice(1);
        const result = eval(args[0]);
        if (result !== undefined && result !== null) console.log(result);
    });' "$@"
}

# The release for a tag as {"id","draft","body"}, or nothing. Lists releases because drafts
# aren't found by tag.
find_release() {
    call GET "/releases?limit=50" | json 'JSON.stringify(json.find(r => r.tag_name === args[1]) ?? null)' "$1" \
        | grep -v '^null$' || true
}

# Puts the tag on this commit (anew) and creates its release with the files
create_release() {
    local tag=$1 name=$2 body=$3 draft=$4 prerelease=$5 release_id file
    ALLOW=404 call DELETE "/tags/$tag" > /dev/null
    post_json /tags "$(json 'JSON.stringify({tag_name: args[1], target: args[2]})' "$tag" "$GITHUB_SHA" <<< '{}')" > /dev/null
    release_id=$(post_json /releases "$(json 'JSON.stringify({tag_name: args[1], name: args[2], body: args[3],
        draft: args[4] === "true", prerelease: args[5] === "true"})' "$tag" "$name" "$body" "$draft" "$prerelease" <<< '{}')" \
        | json 'json.id')
    for file in "${files[@]}"; do
        call POST "/releases/$release_id/assets?name=$(basename "$file")" -F "attachment=@$file" > /dev/null
    done
}

# 1. Rolling pre-release: replace it on this commit
old=$(find_release "$rolling_tag")
if [[ -n $old ]]; then
    call DELETE "/releases/$(json 'json.id' <<< "$old")" > /dev/null
fi
create_release "$rolling_tag" "Development build" \
    "The newest build of the develop branch, from commit $short_sha ($(date -u +%Y-%m-%d)). For trying out new things: it can have bugs, so don't use it with your only copy of real data." \
    false true
echo "Updated the $rolling_tag pre-release"

# 2. The version's draft: create it, or move it to this commit keeping its notes
tag="v$version"
release=$(find_release "$tag")
notes="Release notes go here."
if [[ -n $release ]]; then
    if [[ $(json 'json.draft' <<< "$release") != true ]]; then
        echo "$tag is published already; raise the version in CMakeLists.txt for the next one"
        exit 0
    fi
    notes=$(json 'json.body' <<< "$release")
    call DELETE "/releases/$(json 'json.id' <<< "$release")" > /dev/null
fi
create_release "$tag" "Aion $version" "$notes" true false
echo "The draft for $tag is now at $short_sha; publish it on Codeberg when it's ready"
