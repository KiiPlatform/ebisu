
function recordLog(log, type, done) {
	var bucket=Kii.bucketWithName("log");
	var obj = bucket.createObject();
	obj.set("log", log);
	obj.set("type", type);
	obj.save({
		success : function(theObj) {
					done(theObj);
				},
		failure : function(theObj, err) {
					done(theObj, err);
				}
	});
}



MyKiiRequest = (function(){
	
	function MyKiiRequest(path,done){
	    this._path = "/apps/" + (Kii.getAppID()) +"/"+ path;
		this._done=done;
		
	    this._method = "GET";
	    this._headers = {
	      "accept": "*/*"
	    };
	    this._success = function(json, status) {
			recordLog("kii request","success:"+json+" \n status:"+status,done);
		};
	    this._failure = function(errString, status) {
			recordLog("kii request","failure:"+errString+" \n status:"+status,done);
    	
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
	             return _this._failure(errString, textStatus);
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
            return _this._failure(errString, textStatus);
       };
	   
   	}
	
	MyKiiRequest.prototype.execute=function(callback){
		
		if(callback!=null){
		if(callback["success"]!=null){
			this._success=callback["success"];
		}
		if((callback["failure"]!=null)){
			this._failure=callback["failure"];
		}
	}

	    var url = Kii.getBaseURL() + this._path;
		if(this._sub_path!=null){
			url+="/"+this._sub_path;
			delete this._sub_path;
		};

	    var json_text = null;
		if(this._data!=null){
			json_text=JSON.stringify(this._data);
		}
	    this._headers['x-kii-appid'] = Kii.getAppID();
	    this._headers['x-kii-appkey'] = Kii.getAppKey();
	    this._headers['x-kii-sdk'] = KiiSDKClientInfo.getSDKClientInfo();
	
	    if (this._accept != null) {
	      this._headers['accept'] = this._accept;
	    }
		if(!this._token!=null){
  	      this._headers['Authorization'] = "Bearer " + this._token;
		  delete this._token;
	    }
	
	    if (this._contentType != null) {
	      this._headers['Content-Type'] = this._contentType;
	    }else{
			this._headers['Content-Type']= "application/json";
	    }
		delete this._contentType;
		
		var ajaxParam={};
		ajaxParam["contextType"]=this._contextType;
		ajaxParam["method"]=this._method;
		ajaxParam["success"]=this.onSuccess;
		ajaxParam["error"]=this.onError;
		if(json_text!=null){
			ajaxParam["data"]=json_text;
		}
		ajaxParam["headers"]=this._headers;
	
		$.ajax(url,ajaxParam);
		
	}
	return MyKiiRequest;
})();


UserHelper= (function(){
	
	function UserHelper(user,done){
		this.user=user;
		this._done=done;
		this.groupInfo=this.user.bucketWithName("RelGroupDetail");
	}
	
	UserHelper.prototype.addDefaultGroup=function(groupName,callback){
				var result=this.groupInfo.createObject();
				result.set("groupName",groupName);
				result.set("things",{});
				
				var _done=this._done;
				result.save({
					success:function(json){
						recordLog("add thing to group ",json,_done);
						callback(json);
					},
					failure:function(json,error){
						recordLog("add thing to group fail:",error,_done);
					}
				},true);
	}
	
	UserHelper.prototype.operGroup=function(groupName,things,oper,callback){
		
		var _this=this;
		var fun=function(resultSet){
			if(resultSet.length==0){
			
				_this.addDefaultGroup(groupName,function(result){
					var array=result.get("things");
					if(oper=="add"){
						for(i in things){
							array[things[i]]={"foo":"bar","thingName":things[i]};
						}
					}
					result.set("things",array);
				
					result.save({
						success:function(json){
							callback(things);
						},
						failure:function(json,error){
							recordLog("add thing to group fail:"+error);
						}
					},true);
					
				});
				
			}else{
		
				result=resultSet[0];
				var array=result.get("things");
				
				for(i in things){
					if(oper=="add"){
						array[things[i]]={"foo":"bar","thingName":things[i]};
					}else{
						delete array[things[i]];
					}
				}
				result.set("things",array);				
				result.save({
					success:function(json){
						callback(things);
					},
					failure:function(json,error){
						recordLog("add thing to group fail:"+error,_this._done);
					}
				},true);
			}
			
		
		}
		
		var clause=KiiClause.equals("groupName",groupName);
		
		this.queryGroupList(clause,fun);
		
	}
	
	UserHelper.prototype.queryThingInGroup=function(groupName,callback){
		
		var _done=this._done;
		var fun=function(resultSet){
			if(resultSet.length==0){
				return;
			}
			if(resultSet.length>1){
				recordLog("query for group","mui group have same name",_done);
			}
			callback(resultSet[0].get("things"));
		}
		var clause=KiiClause.equals("groupName",groupName);
		
		this.queryGroupList(clause,fun);
	}
	
	UserHelper.prototype.queryGroupList=function(clause,callback){
		var groups=[];
		var _done=this._done;
		var queryCallbacks = {
		        success: function(queryPerformed, resultSet, nextQuery) {
					for(var i in resultSet){
						groups.push(resultSet[i]);
					}
					
					if(nextQuery==null){
						callback(groups);
					}else{
						this.groupInfo.executeQuery(nextQuery,queryCallbacks);
					}
		        },
      
		        failure: function(queryPerformed, anErrorString) {
					recordLog("query groupList",anErrorString,_done);
		        }
		    };
		this.groupInfo.executeQuery(KiiQuery.queryWithClause(clause),queryCallbacks);
	}
		
	
	return UserHelper;
	
})();

function addThing(params, context, done){
	var thingIDs=params["thingIDs"];
	
	var token=context.getAccessToken();
	
    KiiUser.authenticateWithToken(token, {
         success: function(user) {
		 	var userHelper=new UserHelper(user,done);
			var request=new MyKiiRequest("/things",done);
			request._token=token;
			request._method="PUT";
			
			var addOwner=function(thingIDs){
				if(thingIDs.length==0){
					recordLog("add owner finish","success",done);
					return;
				}
				var thingID=thingIDs.pop();
				request._sub_path="/VENDOR_THING_ID:"+thingID+"/ownership/user:"+user.getID();
				request.execute({
					success:function(){
						addOwner(thingIDs);
					}
				});
			}
			
			userHelper.operGroup("ALL",thingIDs,"add",addOwner);
         },
      
         failure: function(theUser, anErrorString) {
             recordLog("loggin fail",anErrorString,done);
         }
     });
}


function removeThing(params, context, done){
	var thingIDs=params["thingIDs"];
	
	var token=context.getAccessToken();
	
    KiiUser.authenticateWithToken(token, {
         success: function(user) {
		 	var userHelper=new UserHelper(user,done);
			
			var request=new MyKiiRequest("/things",done);
			request._token=token;
			request._method="DELETE";
			
			var removeOwner=function(thingIDs){
				if(thingIDs.length==0){
					recordLog("add owner finish","success",done);
					return;
				}
				var thingID=thingIDs.pop();
				request._sub_path="/VENDOR_THING_ID:"+thingID+"/ownership/user:"+user.getID();
				request.execute({
					success:function(){
						removeOwner(thingIDs);
					}
				});
			}
			
			userHelper.operGroup("ALL",thingIDs,"remove",removeOwner);
         },
      
         failure: function(theUser, anErrorString) {
             recordLog("loggin fail",anErrorString,done);
         }
     });

}

function queryThingsOwner(params, context, done){
	var thingID=params["thingID"];
	
	var request=new MyKiiRequest("/things/VENDOR_THING_ID:"+thingID+"/ownership/");
	request._token=	context.getAccessToken();

	request._method="GET";
	request._contentType="application/vnd.kii.ThingOwnershipRetrievalResponse+json";
	
	var results={};
	
	results["groups"]=[];
	results["users"]=[];
		
	var fillResults=function(users,groups){
		
		if(users.length==0){
			if(groups.length==0){
				done(results);
				return;
			}
		
			var groupID=groups.pop();
			var group= context.getAppAdminContext().groupWithID(groupID);
		
	    	group.refresh({
	        	success: function(theRefreshedGroup) {
	            	results["groups"].push(theRefreshedGroup);
					fillResults([],groups);
	        	},
	        	failure: function(theGroup, anErrorString) {
	            	recordLog("get group fail",anErrorString,done);
	        	}
	    	});
		}else{
		
			var userID=users.pop();
			var user= context.getAppAdminContext().userWithID(userID);
		
	    	user.refresh({
	        	success: function(theRefreshedUser) {
	            	results["users"].push(theRefreshedUser);
					fillResults(users,groups);
	        	},
	        	failure: function(theUser, anErrorString) {
	            	recordLog("get user fail",anErrorString,done);
	        	}
	    	});
		}
	}
	
	request.execute({
		success:function(resultSet){
			var users=resultSet["users"];
			var groups=resultSet["groups"];
			
			fillResults(users,groups);
		}
	});
	
}

function getThingList(params, context, done){
	
	var token=context.getAccessToken();
	
    KiiUser.authenticateWithToken(token, {
         success: function(user) {
		 	var userHelper=new UserHelper(user,done);
			var things={};
			var request=new MyKiiRequest("/things",done);
			
			var listOwner=function(thingObjs){
				
				var thingIDs=Object.keys(thingObjs);
				
				if(thingIDs.length==0){
					done(things);
					return;
				}
				var thingName=thingIDs.pop();
				var thingObj=thingObjs[thingName];
				delete thingObjs[thingName];
				
				things[thingName]=thingObj;
				
				request._token=token;
				request._sub_path="/VENDOR_THING_ID:"+thingName;
				request._method="GET";
				
				request.execute({
					success:function(json){
						things[thingName]["inst"]=json;
						listOwner(thingObjs);
					}
				});
			}
			
			userHelper.queryThingInGroup("ALL",listOwner);
         },
      
         failure: function(theUser, anErrorString) {
             recordLog("loggin fail",anErrorString,done);
         }
     });
	
}




function generSchedule(param, context, done){
	
  KiiUser.authenticateWithToken(context.getAccessToken(), {
	success:function(theUser){
     
	  var obj=KiiObject.objectWithURI(param.uri);
	 
	  obj.refresh({
	      success: function(theObj) {
			  	operateSchedule(theObj,theUser,done,context);
		  },
		  failure: function(theObject, anErrorString) {
		      recordLog(anErrorString, "refresh fail", done);
		  },
		});
	},
	failure:function(theUser,error){
			recordLog(error,"auth fail",done);
	}
  });
}

function operateSchedule(obj,user,done,context){
	
    var admin = context.getAppAdminContext();
	
    var bucket=admin.bucketWithName("Schedule");

    var newSchedule=bucket.createObject();

    var cron=obj.get("cronStr");
    expandFullCron(cron,newSchedule);

    newSchedule.set("name",obj.get("name"));
  
    newSchedule.set("action",obj.get("action"));
    newSchedule.set("targetThing",obj.get("targetThing"));
  
    var groupArray=obj.get("targetGroup");
    if(typeof(groupArray)=="string"){
  	  groupArray=[groupArray];
    }
  	
	
    var groupTopics=[];
    var groupBucket=user.bucketWithName("groupList");
  	
    var callback={
    		success:function(queryPerformed, resultSet, nextQuery){
    			for(var i =0;i<resultSet.length;i++){
    				var groupTopic=resultSet[i].get("groupPushBucket");
  					var groupInfo={};
  					groupInfo.group=groupTopic;
  					groupInfo.user=user.getID();
    				groupTopics.push(groupInfo);
    			}
  				if(nextQuery != null) {
              	   groupBucket.executeQuery(nextQuery, callback);
  				}else{
  					newSchedule.set("targetGroup",groupTopics);
  			   		newSchedule.save({
  			  			success:function(json){
  			  				recordLog("add schedule","success",done);
  			  			},
  			  			failure:function(json, anErrorString){
  			  				recordLog("add schedule",anErrorString,done);
  			  	  		}
  			    	},true);
  				}
    		},
    	  	failure: function(queryPerformed, anErrorString) {
    	  	  	recordLog("query group info",anErrorString,done);
    	  	}
    };
    var query=KiiQuery.queryWithClause(KiiClause.inClause("name", groupArray));
	
    groupBucket.executeQuery(query,callback);
    
}


function  expandFullCron(cron,newSchedule){
	
	var  cronList=cron.split(/\s+/);
	
	/**  1,2,3   1/5   1-5   5*/
	var minutes=expandCron(cronList[0],60);
	newSchedule.set("minutes",minutes);
	var hours=expandCron(cronList[1],24);
	newSchedule.set("hours",hours);
	
	if(cronList[2]!="*"){
		newSchedule.set("days",expandCron(cronList[2],32));
	}else{
		newSchedule.set("days",expandFull(32));
	}
	
	if(cronList[3]!="*"){
		newSchedule.set("months",expandCron(cronList[3],12,1));
	}else{
		newSchedule.set("months",expandFull(12));
	}
	
	if(cronList[4]!="*"){
		var weeks=expandWeek(cronList[4]);
		newSchedule.set("weeks",weeks);
	}else{
		newSchedule.set("weeks",expandFull(7));
	}
	
	return;
	
}

function expandFull(max){
   var result={};

   for(var i=0;i<max;i++){
	   result[String(i)]=true;
   }
   return result;
}

function expandWeek(cron){
	
	var result={};
	var dayMap={
		"mon":1,
		"tue":2,
		"wed":3,
		"thu":4,
		"fri":5,
		"sat":6,
		"sun":0
	}
	
	if(cron.match(/\w+-\w+/)){
		//1-3
		var idx=cron.indexOf("-");
		
		var start=dayMap[cron.substring(0,idx).toLowerCase()];

		var end=dayMap[cron.substring(idx+1).toLowerCase()];
		
		if(start>end){
			end+=7;
		}
		
		if(start!=null&&end!=null){
			for(var i=start;i<=end;i++){
				
				result[String(i%7)]=true;
			}
		}
		
	 }else if (cron.match(/([\w+]+,)+/)){
		//1,3,5
		var array=cron.split(",");
		
		for(var v in  array){
			var i=dayMap[array[v].toLowerCase()];
			if(i!=null){
				result[String(i)]=true;
			}
		}
	}else{
		var i=dayMap[cron.toLowerCase()];
		if(i!=null){
			result[String(i)]=true;
		}
	}
	
	return result;
}


function expandCron(cron,max,offset){
	
	if(offset==null){
		offset=0;
	}
	var result={};
	
	if(cron.match(/\d+-\d+/)){
		//1-3
		var idx=cron.indexOf("-");
		
		var start=Number(cron.substring(0,idx))-offset;
		var end=Number(cron.substring(idx+1))+1-offset;
		
		if(start>end){
			end+=max;
		}
		
		for(var i=start;i<end;i++){
			
			result[String(i%max)]=true;
		}
		
	}else if(cron.match(/\d+\/\d+/)){
		//  0/5
		var idx=cron.indexOf("/");
		
		var start=Number(cron.substring(0,idx))-offset;
		var period=Number(cron.substring(idx+1));
		
		for(var i=start;i<max;i+=period){
			
			result[String(i)]=true;
		}
		
	}else if (cron.match(/([\d+]+,)+/)){
		//1,3,5
		var array=cron.split(",");
		
		for(var i in  array){
			var v=Number(array[i])-offset;
			if(v<max){
				result[String(v)]=true;
			}
		}
	}else if(cron.match(/\d/)){
		result[String(Number(cron)-offset)]=true;
	}
	
	return result;
	
}

function pushMsg(recList,done){
	
	var rec=recList.pop();
	
	var callback={
		success: function(theSavedObject) {
			if(recList.length==0){
				recordLog("fire push msg","success",done);
			}else{
				pushMsg(recList,done);
			}
		},
		failure: function(theObject, anErrorString) {
	  	 	 recordLog("fire push msg fail",anErrorString,done);
		}
	};
	
	rec.save(callback,true);
	
}

function scanSchedule(params, context, done){
	
    var admin = context.getAppAdminContext();
	
	
    var time=new Date();
	
	var min=time.getMinutes();
	var hour=time.getHours();
	var day=time.getDate();
	var month=time.getMonth();
	var week=time.getDay();
	
	 
	var minClause=KiiClause.equals("minutes."+min, true);
	var hourClause=KiiClause.equals("hours."+hour,true);
	
	var dayClause=KiiClause.equals("days."+day,true);
	var monthClause=KiiClause.equals("months."+month,true);
	var weekClause=KiiClause.equals("weeks."+week,true);

	var clause=KiiClause.and(minClause,hourClause,dayClause
		,monthClause,weekClause
	);
	
    var msgList=[];
	var bucket=admin.bucketWithName("schedule");
	
	var  callback={
	      success: function(queryPerformed, resultSet, nextQuery) {

		    for(var i=0; i<resultSet.length; i++) {
		  	  	var obj=resultSet[i];
	  
		  		for(var j in obj.get("targetGroup")){
		 
		  			var groupInfo=obj.get("targetGroup")[j];
		  
		  			var groupBucket=groupInfo.group;
		  			var userID=groupInfo.user;

		  	    	var user=admin.userWithID(userID);
		  	    	var bucket=user.bucketWithName(groupBucket);

		  			var newMsg=bucket.createObject();
		  			newMsg.set("action",obj.get("action"));
		  			newMsg.set("fireType","schedule");
		  			newMsg.set("from",obj.get("name"));
		
		  			msgList.push(newMsg);
		  		}
		  	  	for(var k in obj.get("targetThing")){
		  		  //oper thing bucket.
		  	  	}
		    }

	       if(nextQuery != null) {
	              bucket.executeQuery(nextQuery, callback);
	       }else{
			   if(msgList.length==0){
   					recordLog("finish schedule task","0 fire",done);
			   }else{
				  // recordLog("finish schedule task",msgList,done);
		   		     pushMsg(msgList,done);
			   }
	       }
	      },
      
	      failure: function(queryPerformed, anErrorString) {
	  		recordLog("query schedule fail",anErrorString,done);
	      }
	};
	  
	bucket.executeQuery(KiiQuery.queryWithClause(clause), callback);
    
}


