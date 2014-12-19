from celery import Celery
import json
import MySQLdb as mysql
import httplib
import config
from utils import log

celery = Celery('tasks', broker='redis://localhost:6379/0')

@celery.task
def store_order(order_info):
    try:
        json_obj = json.loads(order_info)
    except ValueError:
        log("Invalid input: " + str(order_info))
        return "Error 1: check input json"
    
    #check db status
    trade_no = json_obj['trade_no']
    db_conn, cursor = get_connection(config.db_config)
    sql = "select id from t_order_info where trade_no='%(trade_no)s' and status not in (1, 3, 5)" % {'trade_no': trade_no}
    rows = cursor.execute(sql)
    if rows != 1:
        db_conn.close()
        log("Cannot find valid order: " + str(order_info))
        return "Error 2: check db status"

    #get app_id
    sql = "select * from t_app_meta where dist_id='%(dist_id)s'" % {"dist_id": json_obj['dist_id']}
    rows = cursor.execute(sql)
    if rows == 1:
        result = cursor.fetchone()
        app_id = result[2]
        app_key = result[3]
    else:
        db_conn.close()
        log("Cannot find app_id for dist: " + str(order_info))
        return "Error 3: cannot find app_id"

    #query trade_no
    status = 1
    if not query_order(trade_no, app_id, app_key):
        if not create_obj(order_info, app_id, app_key):
            status = 2
            log("Error in create App order in kii cloud: " + str(order_info))
    else:
        log("App order has already been stored in kiicloud:" + str(order_info))

    user_id = json_obj['user_id']
    if not query_user_order(trade_no, app_id, app_key, user_id):
        if not create_user_obj(order_info, app_id, app_key, user_id):
            status = 2
            log("Error in create user order: " + str(order_info))
    else:
        log("User order is already stored: " + str(order_info))

    #update db status
    if status != 1:
        return "Error 4: failed to store app or user orders"

    sql = "update t_order_info set status = 1 where trade_no='%(trade_no)s'" % {'trade_no': trade_no}
    rows = cursor.execute(sql)
    if rows ==1:
        db_conn.commit()
        db_conn.close()
        return "Task is done successfully"
    else:
        return "Error 5: updated row is " + str(rows)

def query_user_order(trade_no, app_id, app_key, user_id):
    conn = httplib.HTTPConnection(config.KII_CLOUD_HOST)
    headers = {"Content-type": "application/vnd.kii.QueryRequest+json",
                "x-kii-appid": app_id,
                "x-kii-appkey": app_key,
                "Authorization": "Bearer " + config.ACCESS_TOKEN}
    body = '{"bucketQuery":{"clause":{"type":"eq", "field":"trade_no", "value":"%(trade_no)s"}, "descending": false}}' \
            % {"trade_no": trade_no}
    url = config.USER_ORDER_QUERY_URL % {"app_id": app_id, "user_id": user_id}
    conn.request("POST", url, body, headers)
    response = conn.getresponse()
    if response.status == 200:
        data = response.read()
        json_obj = json.loads(data)
        if json_obj:
            results = json_obj['results']
            if len(results) >= 1:
                return True
    else:
        data = response.read()
        log(url + ": " + str(response.status) 
            + ":" + response.reason 
            + ":" + data)

    return False
 
def create_user_obj(order_info, app_id, app_key, user_id):
    conn = httplib.HTTPConnection(config.KII_CLOUD_HOST)
    content_type = "application/vnd.%(app_id)s.order+json" % {"app_id": app_id}
    headers = {"Content-type": content_type,
                "x-kii-appid": app_id,
                "x-kii-appkey": app_key,
                "Authorization": "Bearer " + config.ACCESS_TOKEN}
    url = config.USER_ORDER_URL % {"app_id": app_id, "user_id": user_id}
    conn.request("POST", url, order_info, headers)
    response = conn.getresponse()
    if response.status == 201:
        data = response.read()
        json_obj = json.loads(data)
        if json_obj and ('objectID' in json_obj) and json_obj['objectID']:
            return True
    else:
        data = response.read()
        log(url + ": " + str(response.status) 
            + ":" + response.reason 
            + ":" + data)

    return False


def create_obj(order_info, app_id, app_key):
    conn = httplib.HTTPConnection(config.KII_CLOUD_HOST)
    content_type = "application/vnd.%(app_id)s.order+json" % {"app_id": app_id}
    headers = {"Content-type": content_type,
                "x-kii-appid": app_id,
                "x-kii-appkey": app_key,
                "Authorization": "Bearer " + config.ACCESS_TOKEN}
    url = config.APP_ORDER_URL % {"app_id": app_id}
    conn.request("POST", url, order_info, headers)
    response = conn.getresponse()
    if response.status == 201:
        data = response.read()
        json_obj = json.loads(data)
        if json_obj and ('objectID' in json_obj) and json_obj['objectID']:
            return True
    else:
        data = response.read()
        log(url + ": " + str(response.status) 
            + ":" + response.reason 
            + ":" + data)

    return False

 
def query_order(trade_no, app_id, app_key):
    conn = httplib.HTTPConnection(config.KII_CLOUD_HOST)
    headers = {"Content-type": "application/vnd.kii.QueryRequest+json",
                "x-kii-appid": app_id,
                "x-kii-appkey": app_key,
                "Authorization": "Bearer " + config.ACCESS_TOKEN}
    body = '{"bucketQuery":{"clause":{"type":"eq", "field":"trade_no", "value":"%(trade_no)s"}, "descending": false}}' \
            % {"trade_no": trade_no}
    url = config.APP_ORDER_QUERY_URL % {"app_id": app_id}
    conn.request("POST", url, body, headers)
    response = conn.getresponse()
    if response.status == 200:
        data = response.read()
        json_obj = json.loads(data)
        if json_obj:
            results = json_obj['results']
            if len(results) >= 1:
                return True
    else:
        data = response.read()
        log(url + ": " + str(response.status) 
            + ":" + response.reason 
            + ":" + data)

    return False

def get_connection(db_config, dict=False):
    """
        get the db's connection
        parmars:
            dict        True means the query results is dict
                        False means the query results is tupple
            db_config   host,port,user,passwd
        return conn,cursor
    """
    conn = mysql.connect(**db_config)
    if dict:
        cursor = conn.cursor(mysql.cursors.DictCursor)
    else:
        cursor = conn.cursor()
    cursor.execute("SET NAMES utf8")
    cursor.execute("SET CHARACTER_SET_CLIENT=utf8")
    cursor.execute("SET CHARACTER_SET_RESULTS=utf8")
    conn.commit()
    return conn, cursor

if __name__ == '__main__':
    order_info = ' {"partner":"2088701768276994","discount":"0.00","payment_type":"1","subject":"Big Sword","trade_no":"2014032133271888","buyer_email":"kiitochina003@gmail.com","gmt_create":"2014-03-21 10:04:46","quantity":"1","out_trade_no":"0TM1LPcUDAcj7zyKbIvGlGn9UTKVDyEiPJokMx6IVratf30pZTAqjXv0TK5izpP4","seller_id":"2088701768276994","trade_status":"TRADE_FINISHED","is_total_fee_adjust":"N","total_fee":"0.01","gmt_payment":"2014-03-21 10:04:47","seller_email":"kiitochina@kii.com","gmt_close":"2014-03-21 10:04:47","price":"0.01","buyer_id":"2088311138446883","use_coupon":"N","dist_id":"7a621d674bb04547","user_id":"2670e865-8fc0-4a69-a99a-f8e97230ec3a","product_id":"3"}'
    print store_order(order_info) 
