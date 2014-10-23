package com.youwill.store.providers;

import android.net.Uri;
import android.provider.BaseColumns;

/**
 * Created by tian on 14-10-3:下午5:28.
 */
public class YouWill {

    public static final String AUTHORITY = "com.youwill.store";

    public static class Application implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/apps");

        public static final String TABLE_NAME = "apps";

        public static final String APP_ID = "app_id";

        public static final String PACKAGE_NAME = "package_name";

        public static final String APP_INFO = "info";

        public static final String NAME = "name";

        public static final String ICON = "icon";

        public static final String VERSION_CODE = "version_code";

        public static final String VERSION_NAME = "version_name";

        public static final String SIZE = "size";

        public static final String MIN_SDK = "min_sdk";

        public static final String PRICE = "price";

        public static final String THUMBNAIL = "thumbnail";

        public static final String DESCRIPTION = "description";

        public static final String DEVELOPER = "developer";

        public static final String DEVELOPER_URL = "developer_url";

        public static final String AGE_CATEGORY = "age_category";

        public static final String SUPPORT_IAP = "support_iap";

        public static final String PROTECT_KEY = "protect_key";

        public static final String CREATE_TIME = "create_time";

        public static final String UPLOAD_TIME = "upload_time";

        public static final String RECOMMEND_TYPE = "recommend_type";

        public static final String RECOMMEND_WEIGHT = "recommend_weight";

        public static final int RECOMMEND_TYPE_LINE1 = 1;

        public static final int RECOMMEND_TYPE_LINE2 = 2;

        public static final int RECOMMEND_TYPE_COVER_FLOW = 3;

        public static final String SEARCH_FIELD = "search_field";

    }

    public static class Developer implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/developers");

        public static final String NAME = "name";

        public static final String URL = "url";

        public static final String COMPANY = "company";

        public static final String DESCRIPTION = "description";

        public static final String DEVELOPER_ID = "developer_id";

    }

    public static class Comment implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/comments");

        public static final String COMMENT_ID = "comment_id";

        public static final String APP_ID = "app_id";

        public static final String USER_ID = "user_id";

        public static final String SCORE = "score";

        public static final String CONTENT = "content";

        public static final String APP_VERSION = "app_version";

    }

    public static class Recommend implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/recommends");

        public static final String APP_ID = "app_id";

        public static final String TYPE = "type";

        public static final String COMMENT = "comment";

        public static final String RECOMMEND_ORDER = "recommend_order";
    }

    public static class Purchased implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/purchased");

        public static final String TABLE_NAME = "purchased";

        public static final String VIEW_NAME = "purchased_view";

        public static final String APP_ID = "app_id";

        public static final String APP_KEY = "app_key";
    }

    public static class Downloads implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/downloads");

        public static final String TABLE_NAME = "downloads";

        public static final String APP_ID = "app_id";

        public static final String DOWNLOAD_ID = "download_id";

        public static final String PACKAGE_NAME = "package_name";
    }

    public static class LocalApps implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/local_apps");

        public static final String TABLE_NAME = "localapps";

        public static final String PACKAGE_NAME = "package_name";

        public static final String VERSION_CODE = "version_code";
    }

    public static class Upgrade implements BaseColumns {

        public static final Uri CONTENT_URI = Uri.parse("content://" + AUTHORITY + "/upgrade");

        public static final String VIEW_NAME = "upgrade";
    }


}
