
PROJECT_ID=${PROJECT_ID:-$CI_PROJECT_ID}

if [ -z "$CI_SERVER_HOST" ]; then
	echo "CI_SERVER_HOST not set"
	exit 1
fi

if [ -z "$PRIVATE_TOKEN" ]; then
	echo "PRIVATE_TOKEN not set"
	exit 1
fi

HOST="https://${CI_SERVER_HOST}/api/v4/projects/${PROJECT_ID}"

glapi_request() {
	PATH="$1"
	shift
	/usr/bin/curl --silent --header "PRIVATE-TOKEN:${PRIVATE_TOKEN}" "${HOST}${PATH}" $@
}

glapi_default_branch() {
	glapi_request "" | jq -r ".default_branch"
}

glapi_list_mrs() {
	glapi_request "/merge_requests?state=opened" | jq -r ".[] | .source_branch"
}

glapi_open_mr() {
	SOURCE_BRANCH="$1"
	TITLE="$2"
	DESCRIPTION="$3"
	ASSIGNEE_ID="3744812" # calebccff
	#TARGET_PROJECT_ID="8065375" # postmarketOS/pmaports
	TARGET_PROJECT_ID="18694862" # sdm845-mainline/pmaports
	TARGET_BRANCH="$(glapi_default_branch)"
	BODY="{
		\"id\": ${PROJECT_ID},
		\"target_project_id\": \"${TARGET_PROJECT_ID}\",
		\"source_branch\": \"${SOURCE_BRANCH}\",
		\"target_branch\": \"${TARGET_BRANCH}\",
		\"remove_source_branch\": true,
		\"title\": \"Draft: ${TITLE}\",
		\"description\": \"${DESCRIPTION}\",
		\"assignee_id\":\"${ASSIGNEE_ID}\"
	}";

	if [ -z "$(glapi_list_mrs | grep $SOURCE_BRANCH)" ]; then
		echo "Creating MR for $SOURCE_BRANCH"
		echo $BODY | jq
		/usr/bin/curl --silent "${HOST}/merge_requests" -X POST \
			--header "PRIVATE-TOKEN:${PRIVATE_TOKEN}" \
			--header "Content-Type: application/json" \
			--data "$BODY"
	else
		echo "MR for $SOURCE_BRANCH already exists"
	fi
}
