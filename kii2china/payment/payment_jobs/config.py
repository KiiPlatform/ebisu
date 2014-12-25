#!/usr/bin/env python
# coding=utf-8

#config.py should not be checked into svn

db_config = {
    'host': '118.102.25.174', 'port': 3306,
    'user': 'root', 'passwd': 'tj74LS0w',
    'db': 'payment',
    'charset': 'utf8',
}

enable_email_notification = True
toaddrs = ['richard.liang@kii.com']

KII_CLOUD_HOST = 'api-cn2.kii.com'
API_ROOT = '/api'
OAUTH2_URL = API_ROOT + '/oauth2/token'

APP_ORDER_ROOT = API_ROOT + "/apps/%(app_id)s/buckets/orders/"
APP_ORDER_URL = APP_ORDER_ROOT + "objects"
APP_ORDER_QUERY_URL = APP_ORDER_ROOT+ "query"

USER_ORDER_ROOT = API_ROOT + "/apps/%(app_id)s/users/%(user_id)s/buckets/orders/"
USER_ORDER_URL = USER_ORDER_ROOT + "objects"
USER_ORDER_QUERY_URL = USER_ORDER_ROOT + "query"

ACCESS_TOKEN = 'dw9uqZRfRWJUNlNSaPonN3mcM8HVJ4RTLaBVYkgaLJQ'

LOG_DIR = "/data/log_payment/tasks/"
