# Building and running the large tests now lives in CMake/CTest:
#
#   cmake --preset debug && cmake --build --preset debug
#   ctest --preset large
#
# What remains here is the one-off bootstrap of the Kii application the large
# tests run against. It is an administrative operation against a live app, not
# part of any build, and it is expected to be run by hand:
#
#   APP_ID=... DEFAULT_SITE=... CLIENT_ID=... CLIENT_SECRET=... make initapp
#
# Needs curl and jq.

initapp:
	# get app admin token
	$(eval ADMIN_TOKEN := $(shell curl -v -X POST -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Content-Type: application/json' https://$(DEFAULT_SITE)/api/oauth2/token -d '{"client_id":"$(CLIENT_ID)","client_secret":"$(CLIENT_SECRET)"}' | jq -r .access_token))
	# create app scope topic
	curl -v -X PUT -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/topics/test_topic
	# deploy server code
	$(eval CODE_VERSION := $(shell curl -v -X POST -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/javascript' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/server-code -d 'function echo(params, context) { return params.message; }' | jq -r .versionID))
	curl -v -X PUT -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: text/plain' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/server-code/versions/current -d '$(CODE_VERSION)'
	# create thing type and firmware version
	curl -v -X PUT -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/vnd.kii.ThingTypeConfigurationRequest+json' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/configuration/thing-types/ltest_thing_type -d '{"verificationCodeFlowStartedByUser":true,"simpleFlow":true,"verificationCodeFlowStartedByThing":true,"verificationCodeTimeout":172800,"verificationCodeLength":9}'
	curl -v -X PUT -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: text/plain' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/configuration/thing-types/ltest_thing_type/firmware-versions/ltest_firmware_version
	# create trait and trait alias
	curl -v -X POST -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/vnd.kii.TraitCreationRequest+json' https://$(DEFAULT_SITE)/thing-if/apps/$(APP_ID)/traits/ltest_trait/versions -d '{"dataGroupingInterval":"15_MINUTES","actions":[],"states":[{"string_field":{"description":"","payloadSchema":{"type":"string"}}}]}'
	curl -v -X PUT -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/vnd.kii.TraitAliasCreationRequest+json' https://$(DEFAULT_SITE)/thing-if/apps/$(APP_ID)/configuration/thing-types/ltest_thing_type/firmware-versions/ltest_firmware_version/aliases/ltest_trait_alias -d '{"traitVersion":"1","trait":"ltest_trait"}'
	# create test user for KHC post test
	curl -v -X POST -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/vnd.kii.RegistrationRequest+json' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/users -d '{"loginName":"pass-1234","password":"1234","displayName":"test-user"}'
	# modify app settings '{"maxLoginAttempts":2,"loginLockPeriodSeconds":3}'
	curl -v -X PATCH -H 'X-Kii-AppID: $(APP_ID)' -H 'X-Kii-AppKey: dummy' -H 'Authorization: Bearer $(ADMIN_TOKEN)' -H 'Content-Type: application/vnd.kii.AppConfigParamsModificationRequest+json' https://$(DEFAULT_SITE)/api/apps/$(APP_ID)/configuration/parameters -d '{"maxLoginAttempts":2,"loginLockPeriodSeconds":3}'

.PHONY: initapp
