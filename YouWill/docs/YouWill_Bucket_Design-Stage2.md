#YouWill Bucket Design
##apps
- appid
- apk_url
- apk_size
- category_id
- category_name
- description
- developer_id
- developer_name
- icon
- minsdk
- name
- package
- price
- protect_key
- thumbnails
- uses_features
- uses_permissions
- version_code
- version_name
- avg_score
- new_category_name
- new_category_id
- upload_time
- has_iap
- short_description
- has_ad
- age_range

##appres
- app_id
- type: 0: icon, 1: thumbnail, 2: cover image
- body: the image file

##appversions
- app_id
- version_name
- version_code
- body: the APK file

##workings
same as apps, but will not publish

##developers
- developer_id
- developer_name

##configure
- alipayPartnerID
- alipayRSAPrivateKey
- alipaySecurityKey
- iapSecurityKey

##product
- name
- price
- valid
- appID
- app
- category_id
- category_name
- developer_id
- developer_name
- consumeType
- order
- description

##latest_apps
- app object
- group_order

##best_rated_apps
- app object
- group_order

##new_app_recommend
- app object
- group_order

##campaign
- app object
- campaign name
- campaign order
- group_order

##campaigns
- campaign name
- campaign id



##categories
- category name
- category id

##reviews
- app_id
- rating
- comment
- user_id
- user_name

##tags
- tag_id
- tag_name

