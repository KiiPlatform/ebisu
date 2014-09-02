package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.dao.ACLOperate;
import com.kii.app.youwill.iap.server.dao.KiiRuntimeException;
import com.kii.app.youwill.iap.server.factory.Product;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.BucketID;
import com.kii.platform.ufp.bucket.BucketType;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.errors.KiiException;
import com.kii.platform.ufp.ufe.acl.Subject;
import com.kii.platform.ufp.ufe.acl.Verb;
import com.kii.platform.ufp.ufe.client.http.stateless.ACLClient;
import com.kii.platform.ufp.user.UserID;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/**
 * Created by ethan on 14-8-15.
 */
@Component
@Product
public class ACLOperateImpl implements ACLOperate {

	@Autowired
	private ACLClient aclClient;

	private UserID getCurrUserID(){
		return context.getCurrUserID();
	}

	@Autowired
	private AppContext context;



	private Map<Verb,Set<Subject>> getBucketACL(String bucketName,ObjectID objID){

			Map<Verb, Set<Subject>>  verbMap= aclClient.get(context.getAccessToken(), context.getCurrScope(),
					BucketType.DEFAULT, new BucketID(bucketName), objID, null);

			return verbMap;


	}

	protected  void addBucketACL(String bucketName,ObjectID objID,UserID userID,Verb verb){
		try {

			aclClient.grant(context.getAccessToken(),context.getCurrScope(),
					BucketType.DEFAULT,new BucketID(bucketName),objID,null,verb,
					userID);

		}catch(KiiException e){
			throw new KiiRuntimeException(e);
		}
	}

	protected  void removeBucketACL(String bucketName,ObjectID objID,UserID userID,Verb verb){
		try {

			aclClient.revoke(context.getAccessToken(),context.getCurrScope(),
					BucketType.DEFAULT,new BucketID(bucketName),objID,null,verb,
					userID);

		}catch(KiiException e){
			throw new KiiRuntimeException(e);
		}
	}

	public void addBucketACLToCurrUser(String bucketName,BucketRight verb){

		addBucketACL(bucketName,null,getCurrUserID(),verb.getVerb());
	}

	public void addObjectACLToCurrUser(String bucketName,ObjectID objID,ObjectRight verb){
		addBucketACL(bucketName,objID,getCurrUserID(),verb.getVerb());

	}

	public void removeBucketACLForCurrUser(String bucketName,BucketRight verb){
		removeBucketACL(bucketName, null, getCurrUserID(), verb.getVerb());

	}

	public void removeObjectACLForCurrUser(String bucketName,ObjectID objID,ObjectRight verb){
		removeBucketACL(bucketName,objID,getCurrUserID(),verb.getVerb());

	}



	public void addBucketACLToSpecUser(String bucketName,BucketRight verb,SpecUser specUser){

		addBucketACL(bucketName,null,specUser.getUserID(),verb.getVerb());
	}

	public void addObjectACLToSpecUser(String bucketName,ObjectID objID,ObjectRight verb,SpecUser specUser){
		addBucketACL(bucketName,objID,specUser.getUserID(),verb.getVerb());

	}

	public void removeBucketACLForSpecUser(String bucketName,BucketRight verb,SpecUser specUser){
		removeBucketACL(bucketName,null,specUser.getUserID(),verb.getVerb());

	}

	public void removeObjectACLForSpecUser(String bucketName,ObjectID objID,ObjectRight verb,SpecUser specUser){
		removeBucketACL(bucketName,objID,specUser.getUserID(),verb.getVerb());

	}


	public Map<ObjectRight,Set<UserID>>  getObjectACLs(String bucketName,ObjectID id) {


		Map<Verb,Set<Subject>>   verbMap=getBucketACL(bucketName,id);

		Map<ObjectRight,Set<UserID>>  verbUserMap=new HashMap<ObjectRight,Set<UserID>>();
		for(Map.Entry<Verb,Set<Subject>> entry:verbMap.entrySet()){

			Set<UserID> setUser=new HashSet<UserID>();
			for(Subject sub:entry.getValue()){
				setUser.add(sub.getUserID());
			}
			verbUserMap.put(ObjectRight.valueOf(entry.getKey().name()),setUser);

		}
		return verbUserMap;

	}
}
