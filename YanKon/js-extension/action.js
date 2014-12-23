KiiUser.prototype.getThing = function(name) {
  
  
	var thing=new KiiThing(name);
	
	thing.setToken(this._accessToken);
	
	return thing;
  
};



FireAction =  (function(){

/*根据指定的目标群组获得thing的集合
  向各个thing scope的LEDControl添加一条记录
  from设为当前用户，action由action字段复制，operateID 为userOperateLog表的对应ID
  */

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
					failure:function(){
						recordLog("save scene  fail:"+error,this.done);
					}
				})
			},
			failure:function(err){
				recordLog("query scene for name fail:"+error,this.done);
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
						recordLog("update group object:"+json,_this.done);
						callback();
						
					},
					failure:function(err){
						recordLog("update group fail",error,_this.done);
					}
					
				});
			},
			failure:function(err){
				recordLog("query group for name fail:"+error,this.done);
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
		
		thing.sendMsg(action,this.user.getDisplayName(),"command",function(){
				_this.fireThing(things,callback);
			});
		
	}
	
	FireAction.prototype.findGroup=function(name,action,callback){
		
		var group=this.user.bucketWithName("GroupInfo");
		
		var clause=KiiClause.equals("groupName",name);
		group.executeQuery(KiiQuery.queryWithClause(clause),{
			success:function(query,results){
				if(results.length==0){
					recordLog("query group fail:","not found",this.done);
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
				recordLog("query group fail:",error,this.done);
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
				recordLog("query scene fail:"+error,this.done);
			}
		});
	}
	
	FireAction.prototype.fireByCommand=function(name,type,action){
		
		var _this=this;
		var onfinish=function(things){
			_this.fireThing(things,function(){
			recordLog("finish thing fire:",_this.done);
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
		
		acl.save({
			success:callback,
			failure:function(error){
				recordLog("add acl fail:"+error,this.done);
			}
		});
	}
	
	
	FireAction.prototype.getPassword=function(thingID){
		return "123456";
	}
	
	FireAction.prototype.registThing=function(thingID,batchName){
		
		
		var thing=new KiiThing(thingID);
		var userID=this.user._uuid;
		
		var _this=this;
		
			var ctrlBucket=thing.getThingBucket("LEDControl");
			
			thing.registThing({"_password":_this.getPassword(thingID),"_thingType":batchName},function(entry){
				thing.addSubscribe(ctrlBucket,function(entry){
					var batch=Kii.bucketWithName("ThingBatchInfo");
					var clause=KiiClause.equals("name",batchName);
					batch.executeQuery(KiiQuery.queryWithClause(clause),{
						success:function(json,result){
									
							var thingBucket=_this.user.bucketWithName("ThingInfo");
							var newThing=thingBucket.createObject();
							newThing.set("thingID",thingID);
							newThing.set("thingSpec",result[0]._customInfo);
									
							newThing.save({
								success:function(entry){
									var acl=ctrlBucket.acl();
									
									thing.runInThing(function(){
										_this.addACL(acl,function(){
											recordLog("success:finish thing obj create",entry);
										});
									});
								},
								failure:function(err){
									recordLog("new thing info fail:"+error,_this.done);
								}
							})
						},
						failure:function(err){
							recordLog("query thing spec fail:"+error,_this.done);
						}
					});		
			});
		},function(err){
			recordLog("the thing had exists:"+error,_this.done);
		});
	
	}
	
	return FireAction;
	
})();