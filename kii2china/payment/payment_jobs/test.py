import json
import MySQLdb as mysql
import httplib
import config
from utils import log


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
    print (rows)
    if rows != 1:
        db_conn.close()
        log("Rows: " + str(rows))
        log("Cannot find valid order: " + str(order_info))
        return "Error 2: check db status"

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
    order_info = '{"partner":"2088701768276994","discount":"0.00","payment_type":"1","subject":"Big Sword","trade_no":"2014032133271888","buyer_email":"kiitochina003@gmail.com","gmt_create":"2014-03-21 10:04:46","quantity":"1","out_trade_no":"0TM1LPcUDAcj7zyKbIvGlGn9UTKVDyEiPJokMx6IVratf30pZTAqjXv0TK5izpP4","seller_id":"2088701768276994","trade_status":"TRADE_FINISHED","is_total_fee_adjust":"N","total_fee":"0.01","gmt_payment":"2014-03-21 10:04:47","seller_email":"kiitochina@kii.com","gmt_close":"2014-03-21 10:04:47","price":"0.01","buyer_id":"2088311138446883","use_coupon":"N","dist_id":"7a621d674bb04547","user_id":"2670e865-8fc0-4a69-a99a-f8e97230ec3a","product_id":"3"}'
    print store_order(order_info) 
