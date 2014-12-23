KiiSchedule = (function(){
	
	function KiiSchedule(user,done){
		this.actionHelper=new FireAction(user,done);
		this.userName=user.getDisplayName();
	}
	
	KiiSchedule.prototype.fillThings=function(name,type,action,onfinish){
		
		switch(type){
			
		case "scene":
			this.actionHelper.findScene(name,action,onfinish);
			break;
		case "group":
			this.actionHelper.findGroup(name,action,onfinish)
			break;
		case "thing":
			var things={};
			for(var t in name){
				things[t]=action;
			}
			onfinish(things);
		}
	}

	KiiSchedule.prototype.operateSchedule=function(obj,admin){
	
	    var bucket=admin.bucketWithName("Schedule");

	    var newSchedule=bucket.createObject();

	    var cron=obj.get("cronStr");
	    this.expandFullCron(cron,newSchedule);

	    newSchedule.set("name",obj.get("name"));
		newSchedule.set("fromUser",this.userName);
		
		var onFinish=function(things){
			
			var target={};
			
			for(var t in things){
				target[t]=obj.get("action");
			}
			
			newSchedule.set("target",target);
	   		newSchedule.save({
	  			success:function(json){
	  				recordLog("add schedule","success",this.done);
	  			},
	  			failure:function(json, anErrorString){
	  				recordLog("add schedule",anErrorString,this.done);
	  	  		}
	    	},true);
			
		}
		
		this.fillThings(obj.get("target"),obj.get("type"),obj.get("action"),onFinish);
		
	}


	KiiSchedule.prototype.expandFullCron=function(cron,newSchedule){
	
		var  cronList=cron.split(/\s+/);
	
		/**  1,2,3   1/5   1-5   5*/
		var minutes=this.expandCron(cronList[0],60);
		newSchedule.set("minutes",minutes);
		var hours=this.expandCron(cronList[1],24);
		newSchedule.set("hours",hours);
	
		if(cronList[2]!="*"){
			newSchedule.set("days",this.expandCron(cronList[2],32));
		}else{
			newSchedule.set("days",this.expandFull(32));
		}
	
		if(cronList[3]!="*"){
			newSchedule.set("months",this.expandCron(cronList[3],12,1));
		}else{
			newSchedule.set("months",this.expandFull(12));
		}
	
		if(cronList[4]!="*"){
			var weeks=this.expandWeek(cronList[4]);
			newSchedule.set("weeks",weeks);
		}else{
			newSchedule.set("weeks",this.expandFull(7));
		}
	
		return;
	
	}

	KiiSchedule.prototype.expandFull=function(max){
	   var result={};

	   for(var i=0;i<max;i++){
		   result[String(i)]=true;
	   }
	   return result;
	}

	KiiSchedule.prototype.expandWeek=function(cron){
	
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


	KiiSchedule.prototype.expandCron=function(cron,max,offset){
	
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
	
	
	
	return KiiSchedule;
	
	
})();

function fireTask(taskList,done){
   if(taskList.length==0){
		recordLog("finish schedule task","0 fire",done);
		return;
   }
   
   var task=taskList.pop();
   var target=task.get("target");
   
   var send=function(targets){
	   var things=Object.keys(targets);
	   
   	   if(things.length==0){
		   fireTask(taskList,done);
   	   }else{
		   var t=things.pop();
		   var thing= new KiiThing(t);
		   var action=targets[t];
		   
		   delete targets[t];
		   
		   thing.fireAction(action,task.get("fromUser"),"schedule",function(){
		   	   		send(targets)
		   		});
   	   	   	
   	   }
   }
   
   send(target);
   
}

function scanTheSchedule(time,done){
	
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
	
    var taskList=[];
	var bucket=Kii.bucketWithName("schedule");
	
	var  callback={
	      success: function(queryPerformed, resultSet, nextQuery) {

		    for(var i=0; i<resultSet.length; i++) {
		  	  	var obj=resultSet[i];
	  	  		taskList.push(obj);
		    }

	       if(nextQuery != null) {
	              bucket.executeQuery(nextQuery, callback);
	       }else{
			   	  fireTask(taskList,done);
			}
	      },
      
	      failure: function(queryPerformed, anErrorString) {
	  		recordLog("query schedule fail",anErrorString,done);
	      }
	};
	  
	bucket.executeQuery(KiiQuery.queryWithClause(clause), callback);
    
}