# -*- coding: utf-8 -*-
__author__ = 'liangyx'

import os.path, json, hashlib
import httplib, sys

APP_ID = 'c99e04f1'
APP_KEY = '3ebdc0472c0c705bc50eaf1756061b8b'
TOKEN = '_2nmV_yKvD3vwEP1dALdQG3Bd6yyDbHF0o7nW3BFF8g'
HOST = 'api-cn2.kii.com'
RES_ROOT = '/Users/liangyx/Downloads/YouWill/p1'
ROOT_FILE = 'apps.csv'

CATEGORIES = {
    '0-3岁':0,
    '3-6岁':1,
    '6-12岁':2,
    '12岁以上':3,
}

is_overwrite = False
publish=False

def process_app_list():
    file_name = os.path.join(RES_ROOT, ROOT_FILE)
    with open(file_name) as f:
        content = f.read()

    lines = content.split('##END##\n')

    for line in lines[1:]:
        app = parse_app_info(line)
        if app:
            app_res =  read_app_res(app['name'])
            log(app_res['meta']['package'])
            publish_app(app, app_res)
        else:
            print 'Error parsing app: ', line



def parse_app_info(line):
    fields = line.split(',')
    app = {
        'name': fields[0],
        'description': fields[1],
        'price': fields[3],
        'developer_name': fields[4],
        'protect_key': fields[5],
        'what_is_new': '',
    }

    category_name =  fields[2]
    if category_name in CATEGORIES:
        app['category_name'] = category_name
        app['category_id'] = CATEGORIES[category_name]
    else:
        print 'Invalid category name', line
    return app


def read_app_res(app_name):
    app_res = {}

    path = os.path.join(RES_ROOT, app_name)

    apk_file = os.path.join(path, app_name + '.apk')
    if os.path.exists(apk_file):
        app_res['apk_file'] = apk_file
        meta_info = get_app_meta(apk_file)
        app_id = generate_app_id(meta_info['package'])
        meta_info['app_id'] = app_id
        app_res['meta'] = meta_info
    else:
        print 'APK file does not exist', app_name

    icon_file = os.path.join(path, 'icon_' + app_name + '.png')
    if os.path.exists(icon_file):
        app_res['icon_file'] = icon_file
    else:
        print 'ICON file does not exist', app_name

    cover_file = os.path.join(path, 'c_' + app_name + '.png')
    if os.path.exists(cover_file):
        app_res['cover_file'] = cover_file

    thumbnails = []
    for i in range(1, 6):
        t  = os.path.join(path, 't_' + app_name + "_" + str(i) + '.png')
        if os.path.exists(t):
            thumbnails.append(t)

    if len(thumbnails) < 4:
        print 'Warning: too less thumbnails', len(thumbnails), app_name
    app_res['thumbnails'] = thumbnails


    return app_res

def publish_app(app, app_res):
    app_object = {
        'name': app['name'],
        'description': app['description'],
        'category_id': app['category_id'],
        'category_name': app['category_name'],
        'price': app['price'],
        'what_is_new': app['what_is_new'],
        'developer_name': app['developer_name'],
        'protect_key': app['protect_key'],
    }
    app_object = dict(app_object.items() + app_res['meta'].items())

    app_object['developer_id'] = create_developer(app['developer_name'])

    res_info = create_resources(app_res)
    app_object = dict(app_object.items() + res_info.items())

    apk_url = create_version(app_res['meta'], app_res['apk_file'])
    app_object['apk_url'] = apk_url
    price = float(app['price'])
    if price > 0:
        iap_id = create_iap_product(app_object)
        app_object['iap_id'] = iap_id

    clause = {
        "bucketQuery":{
        "clause":
            {"type":"eq", "field":"app_id", "value":app_object['app_id']},
        }
    }
    create_object_if_necessary(app_object, 'apps', 'app', clause)

def create_version(params, apk_file):
    app_id = params['app_id']
    version_code = params['version_code']
    object = {
        'app_id': app_id,
        'version_code': version_code,
        'version_name': params['version_name'],
    }

    clause = {
        "bucketQuery": {
            "clause": {
            "type":"and",
            "clauses":[
                {"type":"eq", "field":"app_id", "value":app_id},
                {"type":"eq","field":"version_code", "value":version_code}
            ]
            }
        }
    }
    object_id = create_object_if_necessary(object, 'appversions', 'appversion', clause)
    if object_id:
        apk_url = upload_body('appversions', object_id, apk_file, 'application/octet-stream')
        return apk_url


def create_resources(params):
    app_info = {}

    app_id = params['meta']['app_id']
    object = {
        'app_id': app_id,
        'type': 0, #0 - icon, 1 - thumbnail, 2 - cover image
    }

    clause = {
        "bucketQuery": {
            "clause": {
            "type":"and",
            "clauses":[
                {"type":"eq", "field":"app_id", "value":app_id},
                {"type":"eq","field":"type", "value":0}
            ]
            }
        }
    }

    object_id = create_object_if_necessary(object, 'appres', 'resources', clause)
    if object_id:
        icon_url = upload_body('appres', object_id, params['icon_file'])
        app_info['icon'] = icon_url

    object['type'] = 1
    order = 0
    thumbnail_urls = []
    for thumbnail in params['thumbnails']:
        object['order'] = order
        clause = {
            "bucketQuery": {
                "clause": {
                "type":"and",
                "clauses":[
                    {"type":"eq", "field":"app_id", "value":app_id},
                    {"type":"eq","field":"type", "value":1},
                    {"type":"eq","field":"order", "value": order},
                    ]
                }
            }
        }
        object_id = create_object_if_necessary(object, 'appres', 'resources', clause)
        if object_id:
            url = upload_body('appres', object_id, thumbnail)
            thumbnail_urls.append(url)
        order += 1

    app_info['thumbnails'] = thumbnail_urls

    if 'cover_file' in params:
        object = {
            'app_id': app_id,
            'type': 2,
        }
        clause = {
            "bucketQuery": {
                "clause": {
                "type":"and",
                "clauses":[
                    {"type":"eq", "field":"app_id", "value":app_id},
                    {"type":"eq","field":"type", "value":2},
                    ]
                }
            }
        }
        object_id = create_object_if_necessary(object, 'appres', 'resources', clause)
        if object_id:
            url = upload_body('appres', object_id, params['cover_file'])
            app_info['recommend_image'] = url

    return app_info

def create_iap_product(app):
    app_id = app['app_id']
    product = {
        'price': app['price'],
        "valid": True,
        "consumeType": "permanent",
        "appID": "c99e04f1",
        'name': app['name'],
        'description': app['name'],
        'app': app_id,
        'category_id': app['category_id'],
        'category_name': app['category_name'],
        'developer_id': app['developer_id'],
        'developer_name': app['developer_name'],
    }

    if 'recommend_type' in app:
        product['recommend_type'] = app['recommend_type']
        product['recommend_name'] = app['recommend_name']
        product['recommend_weight'] = app['recommend_weight']

    clause = {
        "bucketQuery":{
        "clause":
            {"type":"eq", "field":"app", "value":app_id},
        }
    }
    object_id = create_object_if_necessary(product, 'product', 'product', clause)
    return object_id


def create_developer(developer_name):
    developer_id = generate_developer_id(developer_name)

    object = {
        'developer_id': developer_id,
        'developer_name': developer_name,
    }

    clause = {
        "bucketQuery":{
        "clause":
            {"type":"eq", "field":"developer_id", "value":developer_id},
        }
    }
    object_id = create_object_if_necessary(object, 'developers', 'developer', clause)
    if not object_id:
        print 'Error create developer info:', developer_name
    return developer_id

def create_object_if_necessary(object, bucket, kii_type=None, clause=None):
    print 'Create object in bucket', bucket

    if clause:
        results = query(clause, bucket)
        if results:
            return results[0]['_id']
    url = '/api/apps/c99e04f1/buckets/%s/objects' % bucket
    conn = httplib.HTTPConnection(HOST)
    if kii_type:
        content_type = 'application/vnd.c99e04f1.%s+json' % kii_type
    else:
        content_type = None
    conn.request('POST', url, json.dumps(object), get_kii_header(content_type))
    response = conn.getresponse()
    print response.status, response.reason
    if response.status == httplib.CREATED:
        content = response.read()
        obj = json.loads(content)
        object_id = obj['objectID']
        return object_id

def update_object(object_id, object):
    headers = get_kii_header('application/json')
    headers['X-HTTP-Method-Override'] = 'PATCH'
    headers['If-None-Match'] = '*'

    url = '/api/apps/c99e04f1/buckets/apps/objects/%s' % object_id
    conn = httplib.HTTPConnection(HOST)
    conn.request('POST', url, json.dumps(object_id), headers)
    response = conn.getresponse()
    if response.status != httplib.OK:
        print 'Failed to update object', object_id, object


def upload_body(bucket, object_id, file_name, content_type='image/jpeg'):
    body = open(file_name)
    url = '/api/apps/c99e04f1/buckets/%s/objects/%s/body' % (bucket, object_id)
    try:
        conn = httplib.HTTPConnection(HOST)
        conn.request('PUT', url, body, get_kii_header(content_type))
        response = conn.getresponse()

        print response.status, response.reason
        if response.status == httplib.OK:
            content =  response.read()
            obj = json.loads(content)
            print obj
            url = publish_body(bucket, object_id)
            return url
    except Exception as e:
        print "Error, ", e.message

def publish_body(bucket, object_id):
    url = '/api/apps/c99e04f1/buckets/%s/objects/%s/body/publish' % (bucket, object_id)
    params = {}
    conn = httplib.HTTPConnection(HOST)
    conn.request('POST', url, json.dumps(params), get_kii_header('application/vnd.kii.objectbodypublicationrequest+json'))
    response = conn.getresponse()
    if response.status == httplib.CREATED:
        content = response.read()
        #print content
        json_obj = json.loads(content)
        return json_obj['url']


def query(clause, bucket):
    url = '/api/apps/c99e04f1/buckets/%s/query' % bucket
    conn = httplib.HTTPConnection(HOST)
    conn.request('POST', url, json.dumps(clause), get_kii_header('application/vnd.kii.QueryRequest+json'))
    response = conn.getresponse()
    print response.status, response.reason
    if response.status == httplib.OK:
        content =  response.read()
        obj = json.loads(content)
        results = obj['results']
        while ('nextPaginationKey' in obj):
            clause['paginationKey'] = obj['nextPaginationKey']
            conn.request('POST', url, json.dumps(clause), get_kii_header('application/vnd.kii.QueryRequest+json'))
            response = conn.getresponse()
            print response.status, response.reason
            content =  response.read()
            obj = json.loads(content)
            results.extend(obj['results'])
        return results


def query_resource(app_id, type=0, order=1):
    pass

def generate_app_id(package):
    md5 = hashlib.md5()
    md5.update(package)
    id = md5.hexdigest()
    return id[0:8]

def generate_developer_id(developer_name):
    md5 = hashlib.md5()
    md5.update(developer_name)
    id = md5.hexdigest()
    return id[0:8]

def get_app_meta(apk_file):
    from subprocess import check_output
    command = '/usr/bin/java -cp /Users/liangyx/Downloads/YouWill/apkparser.jar com.richard.test.ApkDump "%s"'
    str = check_output(command % apk_file, shell=True)
    json_obj = json.loads(str)
    return json_obj['app_meta']

def get_kii_header(content_type=None):
    headers = {
        'Authorization': 'Bearer ' + TOKEN,
        'x-kii-appid': APP_ID,
        'x-kii-appkey': APP_KEY
    }
    if content_type:
        headers['content-type'] = content_type
    else:
        headers['content-type'] = 'application/json'
    return headers

def get_query_versions_clause(app_id, version_code):
    params = {
        "bucketQuery": {
            "clause": {
            "type":"and",
            "clauses":[
                {"type":"eq", "field":"app_id", "value":app_id},
                {"type":"eq","field":"version_code", "value":version_code}
            ]
            }
        }
    }
    return params

def copy_apps(from_bucket, to_bucket):
    allclause = {
        "bucketQuery":{
          "clause":{
            "type":"all"
          }
        }
      }
    apps = query(allclause, from_bucket)

    for app in apps:
        clause = {
            "bucketQuery":{
            "clause":
                {"type":"eq", "field":"app_id", "value":app['app_id']},
            }
        }
        create_object_if_necessary(app, to_bucket, clause=clause)


def dump_download_urls(bucket):
    allclause = {
        "bucketQuery":{
          "clause":{
            "type":"all"
          }
        }
      }
    apps = query(allclause, bucket)
    import codecs
    with codecs.open('download_urls.csv', 'w+', 'utf-8') as f:
        for app in apps:
            f.write('%s,%s\n' %(app['name'], app['apk_url']))


def dump_redirect_urls():
    allclause = {
        "bucketQuery":{
          "clause":{
            "type":"all"
          }
        }
      }
    apps = query(allclause, 'apps')
    import codecs
    with codecs.open('redirect_urls.csv', 'w+', 'utf-8') as f:
        for app in apps:
            f.write('%s,http://www.kii.com/app/%s\n' %(app['name'], app['app_id']))

def log(package):
    with open('pub.log', 'a+') as f:
        f.write(package + '\n')

if __name__=='__main__':
    dump_redirect_urls()