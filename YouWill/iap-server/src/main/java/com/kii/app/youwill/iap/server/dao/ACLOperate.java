package com.kii.app.youwill.iap.server.dao;

import com.kii.platform.ufp.bucket.ObjectID;
import com.kii.platform.ufp.ufe.acl.Verb;
import com.kii.platform.ufp.user.UserID;

import java.util.Map;
import java.util.Set;

/**
 * Created by ethan on 14-9-1.
 */
public interface ACLOperate {

	public static enum BucketRight{

		CreateObject(Verb.CREATE_OBJECTS_IN_BUCKET),
		QueryObject(Verb.QUERY_OBJECTS_IN_BUCKET),
		DeleteObject(Verb.DROP_BUCKET_WITH_ALL_CONTENT);

		private Verb verb;

		public Verb getVerb(){
			return verb;
		}


		BucketRight(Verb verb){
			this.verb=verb;
		}

	}




	public static enum ObjectRight{
		Read(Verb.READ_EXISTING_OBJECT),
		Write(Verb.WRITE_EXISTING_OBJECT);

		private Verb verb;

		public Verb getVerb(){
			return verb;
		}
		ObjectRight(Verb verb){
			this.verb=verb;
		}
	}

	public void addBucketACLToCurrUser(String bucketName,BucketRight verb);

	public void addObjectACLToCurrUser(String bucketName,ObjectID objID,ObjectRight verb);

	public void removeBucketACLForCurrUser(String bucketName,BucketRight verb);

	public void removeObjectACLForCurrUser(String bucketName,ObjectID objID,ObjectRight verb);

	public enum SpecUser{
		ANONYMOUS_USER,ANY_AUTHENTICATED_USER;

		public UserID getUserID(){
			return new UserID(this.name());
		}
	}

	public void addBucketACLToSpecUser(String bucketName,BucketRight verb,SpecUser specUser);

	public void addObjectACLToSpecUser(String bucketName,ObjectID objID,ObjectRight verb,SpecUser specUser);

	public void removeBucketACLForSpecUser(String bucketName,BucketRight verb,SpecUser specUser);

	public void removeObjectACLForSpecUser(String bucketName,ObjectID objID,ObjectRight verb,SpecUser specUser);


	public Map<ObjectRight,Set<UserID>> getObjectACLs(String bucketName,ObjectID id) ;
}
