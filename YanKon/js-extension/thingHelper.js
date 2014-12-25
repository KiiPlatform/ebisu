KiiBucket.prototype._generatePath = function() {
  var path;

  if (this._user != null) {
    path = "/users/" + (this._user.getUUID()) + "/buckets/" + this._bucketName;
  } else if (this._group != null) {
    path = "/groups/" + (this._group.getUUID()) + "/buckets/" + this._bucketName;
  } else if(this._thing !=null){
	path="/things/"+(this._thing._id)+"/buckets/"+this._bucketName;
  } else{
    path = "/buckets/" + this._bucketName;
  }
  return path;
};


KiiACL.prototype.aclPath = function() {
  var bucket, bucketName, group, object, objectId, path, user;
  if (this._parent instanceof KiiObject) {
    object = this._parent;
    if (object.getBucket().getUser() != null) {
      user = object.getBucket().getUser();
    } else if (object.getBucket().getGroup() != null) {
      group = object.getBucket().getGroup();
    } else if (object.getBucket()._thing != null ){
		thing=object.getBucket().object.getBucket()._thing._id;
    }
    bucketName = object.getBucket().getBucketName();
    objectId = object.getUUID();
  } else if (this._parent instanceof KiiBucket) {
    bucket = this._parent;
    if (bucket.getUser() != null) {
      user = bucket.getUser();
    } else if (bucket.getGroup() != null) {
      group = bucket.getGroup();
    }else if(bucket._thing !=null){
		thing = bucket._thing._id;
    }
    bucketName = bucket.getBucketName();
  } else {
    root.Kii.logger("Invalid ACL parent. Must belong to a KiiObject");
  }
  path = "/";
  if (group != null) {
    path += "groups/" + (group.getUUID()) + "/";
  } else if (user != null) {
    path += "users/" + (user.getUUID()) + "/";
  } else if(thing != null){
	  path += "things/"+thing+"/";
  }
  if (objectId != null) {
    path += "buckets/" + bucketName + "/objects/" + objectId + "/acl";
  } else {
    path += "buckets/" + bucketName + "/acl";
  }
  return path;
};

KiiObject.prototype._getPath = function() {
  var path;
  if (this._bucket.getUser() != null) {
    path = "/users/" + (this._bucket.getUser().getUUID()) + "/buckets/" + (this._bucket.getBucketName()) + "/objects/";
  } else if (this._bucket.getGroup() != null) {
    path = "/groups/" + (this._bucket.getGroup().getUUID()) + "/buckets/" + (this._bucket.getBucketName()) + "/objects/";
  } else if (this._bucket._thing !=null ){
	  path="/things/"+this._bucket._thing._id+"/buckets/"+(this._bucket.getBucketName())+"/objects/";
  }else {
    path = "/buckets/" + (this._bucket.getBucketName()) + "/objects/";
  }
  if (this._uuid != null) {
    path += this._uuid;
  }
  return path;
};

KiiThing =  (function(){
	
	
	function KiiThing(name){
		
		this._name=name;
		
		this._request=new MyKiiRequest("things/");
		this._request._method="POST";
		
		this.authRequest=new MyKiiRequest("oauth2/token/");
		this.authRequest._contentType="application/vnd.kii.OauthTokenRequest+json";
		this.authRequest._method="POST";
		
		this._id="VENDOR_THING_ID:"+name;
		this._name=name;
		
		this._owner_request=new MyKiiRequest("things/VENDOR_THING_ID:"+name+"/ownership/");
		
	}
	
	KiiThing.prototype.setToken=function(token){
		
		this._request._token=token;
		this._owner_request._token=token;
		
	}
	
	KiiThing.prototype.registThing=function(thingData,onRegist,onExists){
		
		this._request._contentType="application/vnd.kii.ThingRegistrationAndAuthorizationRequest+json";
		this._request._method="POST";
		
		this._request._anonymous = true;
		this._request._data=thingData;
		this._request._data["_vendorThingID"]=this._name;
		
		var _this=this;
		this._request.execute({
			success:function(json,status){
				_this.setToken(json._accessToken);
				onRegist(json);
			},
			failure:function(jqResp,str,err){
				if(str=="409"){
					_this.loginThing(thingData["_password"],function(){
						onRegist(null);
					});
					
				} 
			}
		});
	}
	
	
	KiiThing.prototype.loginThing=function(pwd,onLogin){
		
		var param={};
		//{"username":"VENDOR_THING_ID:demoid10081508","password":"123456","grant_type": "password"}
		param["username"]="VENDOR_THING_ID:"+this._name;
		param["password"]=pwd;
		param["grant_type"]="password";
		
		this.authRequest._data=param;
		
		var _this=this;
		this.authRequest.execute({
			
			success:function(json,status){
				_this.setToken(json["access_token"]);
				
				onLogin(this);
			}
		});
		
	}
	
	KiiThing.prototype.getThingInfo=function(onReceive){
		
		this._request._method="GET";
		
		this._request._sub_path=this._id;
		
		this._request.execute({
			success:function(json,status){
				onReceive(json);
			}
		});
	}
	
	KiiThing.prototype.removeUser=function(userID,callback){
		this._owner_request._sub_path="user:"+id;
		
		this._owner_request._method="DELETE";
		
		this._owner_request.execute({
			success:callback
		});
	}
	
	KiiThing.prototype.removeGroup=function(groupID,callback){
		this._owner_request._sub_path="group:"+id;
		
		this._owner_request._method="DELETE";
		
		this._owner_request.execute({
			success:callback
		});
	}
	
	KiiThing.prototype.addGroup=function(groupID,callback){
		this._owner_request._sub_path="group:"+id;
		
		this._owner_request._method="PUT";
		
		
		this._owner_request.execute({
			success:callback
		});
	}
	
	KiiThing.prototype.addUser=function(userID,callback){
		this._owner_request._sub_path="user:"+id;
		
		this._owner_request._method="PUT";
		
		
		this._owner_request.execute({
			success:callback
		});
	}
	
	
	KiiThing.prototype.getThingBucket=function(bucketName){
		var bucket=Kii.bucketWithName(bucketName);
		bucket._thing=this;
		return bucket;
	}
	
	KiiThing.prototype.addSubscribe=function(bucket,callback){
		///apps/{appID}/users/{userID}/buckets/{bucketID}/filters/{filterID}/push/subscriptions/users/{userID}		
		this._request._sub_path=this._id+"/buckets/"+bucket.getBucketName()+"/filters/all/push/subscriptions/things"
		
		this._request._data=null;
		this._request.execute({
			
			success:function(json,status){
				callback();
			},
			failure:function(jqResp,str,err){
				if(str=="409"){
					callback();
				} 
			}
		});
	}
	
	KiiThing.prototype.runInThing=function(func){
		
		var oldUser=Kii.getCurrentUser();
		var thing={};
		thing._uuid=this._id;
		thing._accessToken=this._request._token;
		
		Kii.setCurrentUser(thing);
		
		func();
		
		Kii.setCurrentUser(oldUser);
		
	}
	
	KiiThing.prototype.fireAction=function(action,from,cmd,callback){
		var bucket=this.getThingBucket("LEDControl");
		
		var obj=bucket.createObject();
		
		obj.set("action",action);
		obj.set("from",from);
		obj.set("type",cmd);
		
		var _this=this;
		obj.save({
			success:function(entry){
				callback();
			},
			failure:function(err){
				callback();
			}
		});
	}
	
	

	return KiiThing;
	
})();
