#!/bin/sh

set -e
PKG="$1"
TAG="$2"
BRANCH="$(basename $PKG)/$TAG"

# XXX: pass in needed vars rather than this awful hack
# sudo -E preserves environment
USER=pmos
HOME=/home/$USER

grev() {
	git rev-parse --verify $1
}

BASEDIR="$PWD"
cd /home/pmos/.local/var/pmbootstrap/cache_git/pmaports

git config --local user.name "SDM845 CI"
git config --local user.email "sdm845-ci@blah.com"

if [ "$(grev $BRANCH || true)" != "" ]; then
	echo "Branch $BRANCH already exists, ammending"
	git stash push . || true
	git checkout $BRANCH
	git stash pop || true
	git add -A
	git commit --amend --no-edit
	echo "Amended pmaports commit:"
else
	git add -A
	git commit -m "$PKG: Upgrade to $TAG"
	echo "Created pmaports commit:"
fi
git show -1
git remote remove ssh_origin || true # local repo state may be cached
git remote add ssh_origin "git@$CI_SERVER_HOST:sdm845-mainline/pmaports.git"
eval $(ssh-agent -s)
echo "$PMAPORTS_SSH_PUSH_KEY" | tr -d '\r' | ssh-add -
BRANCH="$(basename $PKG)-$TAG"
git checkout -b "$BRANCH"
GIT_SSH_COMMAND="ssh -o StrictHostKeyChecking=no" git push --force ssh_origin HEAD:$BRANCH

PRIVATE_TOKEN="$SDM845_CI_TOKEN"
# pmaports project ID
PROJECT_ID="18694862"
. $BASEDIR/.ci/gitlab_api.sh

glapi_open_mr "$BRANCH" "$PKG: Upgrade to $TAG" "Cc: @sdm845-mainline @calebccff\n\nTo test (not all required):\n - [ ] shift-axolotl\n - [ ] oneplus-enchilada\n - [ ] oneplus-fajita\n - [ ] xiaomi-beryllium"
