package com.kii.app.youwill.iap.server.dao.impl;

import com.kii.app.youwill.iap.server.factory.TestProfile;
import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.ufe.acl.Verb;
import com.kii.platform.ufp.user.UserID;
import org.springframework.stereotype.Component;

/**
 * Created by ethan on 14-9-1.
 */
@Component
@TestProfile
public class ACLOperateImplForTest extends ACLOperateImpl {

	protected  void addBucketACL(String bucketName,ObjectID objID,UserID userID,Verb verb){
		return;
	}

	protected  void removeBucketACL(String bucketName,ObjectID objID,UserID userID,Verb verb){
		return;
	}
}
