KiiUser.prototype.getThing = function(name) {
  
  
	var thing=new KiiThing(name,null);
	
	thing.setToken(this._accessToken);
	
	return thing;
  
};


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

//==============================================
//custom request
//==============================================

MyKiiRequest = (function(){
	
	function MyKiiRequest(path){
	    this._path = "/apps/" + (Kii.getAppID()) +"/"+ path;
		
	    this._method = "GET";
	    this._headers = {
	      "accept": "*/*"  
	    };
	    this._success = function(json, status) {
			console.log("success:"+json+" \n status:"+status);
		};
	    this._failure = function(errString, status) {
			console.log("failure:"+errString+" \n status:"+status);
    	
	    };
	
		var _this=this;
	    
	    this.onSuccess=function(anything, textStatus,jqXHR) {
			  
	        if ((200 <= (_ref1 = jqXHR.status) && _ref1 < 400)) {
	            if (jqXHR.status==204) {
	              return _this._success(null, textStatus);
	            } else {
					if (anything.errorCode != null) {
					  var errString=anything.errorCode+anything.message;
					  return _this._failure(errString, jqXHR.status, anything.errorCode);
	                } else {
	                  return _this._success(anything, textStatus);
	                }
	            }
	        } else {
	            var errString = xhr.status + " : " + _this._path;
	            var json = JSON.parse(decodeURIComponent(jqXHR.responseText));
	            if (json.errorCode != null) {
	                errString = json.errorCode+json.message;
	             }
	             return _this._failure(errString, jqXHR.status,json);
	       }
	    };
		
		this.onError=function(jqXHR,textStatus,errorThrown){
            var errString = textStatus + " : "  + _this._path;
            var json = JSON.parse(decodeURIComponent(jqXHR.responseText));
            if (json != null) {
              if (json.errorCode != null) {
                  errString = json.errorCode;
                if (json.message != null) {
                  errString += ": " + json.message;
                }
              }
            }
            return _this._failure(errString, jqXHR.status,json);
       };
	   
   	}
	
	MyKiiRequest.prototype.execute=function(callback){
		
		if(callback["success"]!=null){
			this._success=callback["success"];
		}
		if((callback["failure"]!=null)){
			this._failure=callback["failure"];
		}

	    var url = Kii.getBaseURL() + this._path;
		if(this._sub_path!=null){
			url+="/"+this._sub_path;
			delete this._sub_path;
		};
	
	
	    this._headers['x-kii-appid'] = Kii.getAppID();
	    this._headers['x-kii-appkey'] = Kii.getAppKey();
	    this._headers['x-kii-sdk'] = KiiSDKClientInfo.getSDKClientInfo();
	
	    if (this._accept != null) {
	      this._headers['accept'] = this._accept;
	    }
		if(this._token!=null){
  	      this._headers['Authorization'] = "Bearer " + this._token;
		  // delete this._token;
	    }
	
	    if (this._contentType != null) {
	      this._headers['Content-Type'] = this._contentType;
	    }else{
			this._headers['Content-Type']= "application/json";
	    }
		// delete this._contentType;
		
		var ajaxParam={};
		ajaxParam["method"]=this._method;
		ajaxParam["success"]=this.onSuccess;
		ajaxParam["error"]=this.onError;
		if(this._data!=null){
			ajaxParam["data"]=JSON.stringify(this._data);
		}
		ajaxParam["headers"]=this._headers;
	
		$.ajax(url,ajaxParam);
		
	}
	
	
	return MyKiiRequest;
})();



function recordLog(log,done,type) {
	var bucket=Kii.bucketWithName("log");
	var obj = bucket.createObject();
	
	if(type==null){
		type="success";
	}
	obj.set("log", log);
	obj.set("type", type);
	
	var result={};
	result[type]=JSON.stringify(log);
	var _done=done;
	obj.save({
		success : function(theObj) {
					_done(result);
				},
		failure : function(theObj, err) {
					_done(result);
				}
	});
}

//=================================
//action obj
//=================================

FireAction =  (function(){

	function FireAction(user,done){
		this.user=user;
		this.done=done;
	}
	
	FireAction.prototype.mergeObj=function(src,des){
		
		for(var k in des){
			src[k]=des[k];
		}
		return src;
		
	}
	
	FireAction.prototype.addScene=function(name,member,type,action,callback){
		var scene=this.user.bucketWithName("SceneInfo");
		
		var _this=this;
		
		var clause=KiiClause.equals("sceneName",name);
		scene.executeQuery(KiiQuery.queryWithClause(clause),{
			success:function(query,resultSet){
				var s=null;
				if(resultSet.length==0){
					s=scene.createObject();
					s.set("members",{});
					s.set("sceneName",name);
				}else{
					s=resultSet[0];
				}
				
				var m={};
				m["defaultAction"]=action;
				m["type"]=type;
				
				var ms=s.get("members");
				ms[member]=m;
				
				s.set("members",ms);
				
				s.save({
					success:function(json){
						callback(json);
					},
					failure:function(error){
						recordLog(error,_this.done,"failure");
					}
				})
			},
			failure:function(err){
				recordLog(error,_this.done,"failure");
			}
				
		});
		
	}
	
	FireAction.prototype.addGroup=function(name,thingID,action,callback){
		var group=this.user.bucketWithName("GroupInfo");
		
		var _this=this;
		
		var clause=KiiClause.equals("groupName",name);
		
		group.executeQuery(KiiQuery.queryWithClause(clause),{
			success:function(query,resultSet){
				
				
				var g=null;
				if(resultSet.length==0){
					g=group.createObject();
					g.set("groupName",name);
					g.set("things",{});
				}else{
					g=resultSet[0];
				}
				if(action!=null){
					g.set("defaultAction",action);
				}
				
				var things=g.get("things");
				
				things[thingID]=true;
				
				g.set("things",things);
				
				g.save({
					success:function(json){
						recordLog(json,_this.done);
						callback();
						
					},
					failure:function(err){
						recordLog(error,_this.done,"failure");
					}
					
				});
			},
			failure:function(err){
				recordLog(error,_this.done,"failure");
			}
		});
	}
	
	FireAction.prototype.fireThing=function(things,callback){
		
		var thingIDs=Object.keys(things);
		if(thingIDs.length==0){
			callback();
			return;
		}
		var thingID=thingIDs.pop();
		
		var action=things[thingID];
		delete things[thingID];
		
		var thing=this.user.getThing(thingID);
		
		var _this=this;
		
		thing.done=this.done;
		
		thing.fireAction(action,_this.user.getDisplayName(),"command",
			function(){
				_this.fireThing(things,callback);
			}
		);
		
	}
	
	FireAction.prototype.findGroup=function(name,action,callback){
		
		var group=this.user.bucketWithName("GroupInfo");
		
		var clause=KiiClause.equals("groupName",name);
		var _this=this;
		group.executeQuery(KiiQuery.queryWithClause(clause),{
			success:function(query,results){
				if(results.length==0){
					recordLog("query group fail:not found",this.done,"failure");
					return;
				}
				var entry=results[0];
				var things=entry.get("things");
				var objs={};
				for(var t in things){
					objs[t]=action;
				}
				callback(objs);
			},
			failure:function(error){
				recordLog(error,_this.done,"failure");
			}
		});
	}
	
	FireAction.prototype.findScene=function(name,action,callback){
		var bucket=this.user.bucketWithName("SceneInfo");
		var clause=KiiClause.equals("sceneName",name);
		
		var _this=this;
		bucket.executeQuery(KiiQuery.queryWithClause(clause),{
			success:function(query,result){
				var things={};
				
				if(result.length==0){
					callback(things);
					return;
				}
				var entry=result[0];
				var members=entry.get("members");
				
				var objs=[];
				
				for(var n in members){
					
					var val=members[n];
					val["name"]=n;
					
					objs.push(val);
					
				}
				
				var reduce=function(objList){
					
					if(objList.length==0){
						callback(things);
						return;
					}
					
					var obj=objList.pop();
					var objName=obj["name"];
					
					if(obj["type"]=="thing"){
						var defaultAct=val["defaultAction"];
						things[objName]=_this.mergeObj(defaultAct,action);
						reduce(objList);
					}else{
						var defaultAct=val["defaultAction"];
						_this.findGroup(objName,_this.mergeObj(defaultAct,action),function(thingsInGroup){
							things=_this.mergeObj(thingsInGroup);
							reduce(objList);
						});
					}
				};
				
				reduce(objs);
			},
			failure:function(error){
				recordLog(error,_this.done,"failure");
			}
		});
	}
	
	FireAction.prototype.fireByCommand=function(name,type,action){
		
		var _this=this;
		var onfinish=function(things){
			_this.fireThing(things,function(){
				recordLog("finish thing fire:"+things,_this.done);
			});
		};
		
		switch(type){
			
		case "scene":
			this.findScene(name,action,onfinish);
			break;
		case "group":
			this.findGroup(name,action,onfinish)
			break;
		case "thing":
			var things={};
			things[name]=action;
			onfinish(things);
		}
	}
	
	FireAction.prototype.addACL=function(acl,callback){
		
		var entry=KiiACLEntry.entryWithSubject(this.user, KiiACLAction.KiiACLBucketActionCreateObjects);
		
		acl.putACLEntry(entry);
		
		var _this=this;
		acl.save({
			success:callback,
			failure:function(error){
				recordLog(error,_this.done,"add_acl_failure");
			}
		});
	}
	
	
	FireAction.prototype.getPassword=function(thingID){
		return "123456";
	}
	
	FireAction.prototype.postRegistThing=function(thingID,batchName){
		
		var thing=new KiiThing(thingID,this.done);
		var _this=this;
		
		var ctrlBucket=thing.getThingBucket("LEDControl");
		
		thing.loginThing(this.getPassword(thingID),function(){
			thing.addSubscribe(ctrlBucket,function(entry){
				var batch=Kii.bucketWithName("ThingBatchInfo");
				var clause=KiiClause.equals("name",batchName);
				batch.executeQuery(KiiQuery.queryWithClause(clause),{
					success:function(json,result){
							
						var thingBucket=_this.user.bucketWithName("ThingInfo");
						var newThing=thingBucket.createObject();
						newThing.set("thingID",thingID);
						var batchInfo={};
						if(result.length>0){
							batchInfo=result[0]._customInfo;
						}
						newThing.set("thingSpec",batchInfo);
							
						newThing.save({
							success:function(entry){
								var acl=ctrlBucket.acl();
							
								thing.runInThing(function(){
									_this.addACL(acl,function(){
										recordLog("success:finish thing obj create",_this.done);
									});
								});
							},
							failure:function(err){
								recordLog(err,_this.done,"save_thing_failure");
							}
						})
					},
					failure:function(err){
						recordLog(err,_this.done,"query_thing_spec_failure");
					}
			});		
		});
	  }); 
    }
	
	FireAction.prototype.registThing=function(thingID,batchName){
		
		
		var thing=new KiiThing(thingID,this.done);
		var userID=this.user._uuid;
		
		var _this=this;
		
		
		thing.registThing(
			{"_password":_this.getPassword(thingID),"batchName":batchName,"_thingType":"Light"},
			function(entry){
				_this.postRegistThing(thingID,batchName);
		},function(err){
			recordLog(err,_this.done,"thing_exist");
		});
	
	}
	
	return FireAction;
	
})();



//===================================================
//thing obj
//===================================================


KiiThing =  (function(){
	
	
	function KiiThing(name,done){
		
		this.done=done;
		this._name=name;
		
		this._request=new MyKiiRequest("things/");
		this._request._method="POST";
		
		this.authRequest=new MyKiiRequest("oauth2/token/");
		this.authRequest._contentType="application/vnd.kii.OauthTokenRequest+json";
		this.authRequest._method="POST";
		
		this._id="VENDOR_THING_ID:"+name;
		
		this._owner_request=new MyKiiRequest("things/VENDOR_THING_ID:"+name+"/ownership/");
		
	}
	
	KiiThing.prototype.setToken=function(token){
		
		this._request._token=token;
		this._owner_request._token=token;
		
	}
	
	KiiThing.prototype.registThing=function(thingData,onRegist,onExists,onFailure){
		
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
						onExists(str);
					});
				} else {
					recordLog(err,_this.done,"thing_regist_failure");
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
				
				onLogin(_this);
			},
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_login_failure");
			}
		});
		
	}
	
	KiiThing.prototype.getThingInfo=function(onReceive){
		
		this._request._method="GET";
		
		this._request._sub_path=this._id;
		var _this=this;
		
		this._request.execute({
			success:function(json,status){
				onReceive(json);
			},
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_getInfo_failure");
			}
		});
	}
	
	KiiThing.prototype.removeUser=function(userID,callback){
		this._owner_request._sub_path="user:"+id;
		
		this._owner_request._method="DELETE";
		var _this=this;
		
		this._owner_request.execute({
			success:callback,
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_removeUser_failure");
			}
		});
	}
	
	KiiThing.prototype.removeGroup=function(groupID,callback){
		this._owner_request._sub_path="group:"+id;
		
		this._owner_request._method="DELETE";
		var _this=this;
		
		this._owner_request.execute({
			success:callback,
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_removeGroup_failure");
			}
		});
	}
	
	KiiThing.prototype.addGroup=function(groupID,callback){
		this._owner_request._sub_path="group:"+id;
		
		this._owner_request._method="PUT";
		
		var _this=this;
		
		this._owner_request.execute({
			success:callback,
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_addGroup_failure");
			}
		});
	}
	
	KiiThing.prototype.addUser=function(userID,callback){
		this._owner_request._sub_path="user:"+id;
		
		this._owner_request._method="PUT";
		
		var _this=this;
		this._owner_request.execute({
			success:callback,
			failure:function(jsResp,status,err){
				recordLog(err,_this.done,"thing_addUser_failure");
			}
		});
	}
	
	
	KiiThing.prototype.getThingBucket=function(bucketName){
		var bucket=Kii.bucketWithName(bucketName);
		bucket._thing=this;
		return bucket;
	}
	
	KiiThing.prototype.addSubscribe=function(bucket,callback){
		///apps/{appID}/users/{userID}/buckets/{bucketID}/filters/{filterID}/push/subscriptions/users/{userID}		
		var _this=this;
		this._request._sub_path=this._id+"/buckets/"+bucket.getBucketName()+"/filters/all/push/subscriptions/things"
		
		this._request._data=null;
		this._request.execute({
			
			success:function(json,status){
				callback();
			},
			failure:function(jqResp,str,err){
				if(str=="409"){
					callback();
				}else{
					recordLog(err,_this.done,"thing_addSubscribe_failure");
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
				recordLog(err,_this.done,"thing_fire_failure");
			}
		});
	}
	
	return KiiThing;
	
})();


//=====================================
//point
//=====================================


function postRegistLamp(params, context, done){

	var token=context.getAccessToken();
	
	KiiUser.authenticateWithToken(token, {
		success:function(user){
	
			var action = new FireAction(user,done);
		
			var thingID=params.thingID;
			var batchName=params.batchName;
	
			action.postRegistThing(thingID,batchName);
		},
		failure:function(error){
			recordLog(error,done,"loggin_failure");
		}
	});
	
}

function fireLamp(params,context,done){
	
	var token=context.getAccessToken();
	
	KiiUser.authenticateWithToken(token, {
		success:function(user){
	
			var action = new  FireAction(user,done);
	
			var thing=params.thingID;
			var act=params.action;
			
			action.fireByCommand(thing,"thing",act);
				
		},
		failure:function(error){
			recordLog(error,done,"loggin_failure")
		}
	});
	
}

function loginThing(params,context,done){
	
	var thingID=params.thingID;
	
	var thing=new KiiThing(thingID,done);
	
	thing.loginThing("123456",function(){
		recordLog("login finish",done);
	});
	
	
}


function registThing(params,context,done){
	
	
	var _headers={};
	
    _headers['x-kii-appid'] = Kii.getAppID();
    _headers['x-kii-appkey'] = Kii.getAppKey();
    _headers['x-kii-sdk'] = KiiSDKClientInfo.getSDKClientInfo();

    _headers['Content-Type'] = "application/vnd.kii.ThingRegistrationAndAuthorizationRequest+json";
    // delete this._contentType;
	
	var _data={};
	_data["_vendorThingID"]=params.thingID;
	_data["_password"]="123456";
	
	
	var ajaxParam={};
	ajaxParam["method"]="POST";
	ajaxParam["data"]=JSON.stringify(_data);
	ajaxParam["headers"]=_headers;
	
	ajaxParam["success"]=function(json,status){
			done({"success":json});
	};
	
	ajaxParam["error"]=function(jqResp,str,err){
		done({"status":str,"error":err,"jqResp":jqResp})
	};

	var uri=Kii.getBaseURL() +"/apps/" + Kii.getAppID() +"/things/" ;
	$.ajax(uri,ajaxParam);
	
}