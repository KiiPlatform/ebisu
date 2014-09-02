package com.kii.app.youwill.iap.server.dao;

import com.kii.app.youwill.iap.server.factory.TokenInfo;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.bucket.*;
import com.kii.platform.ufp.errors.KiiException;
import com.kii.platform.ufp.ufe.ObjectCreationResponse;
import com.kii.platform.ufp.ufe.QueryRequest;
import com.kii.platform.ufp.ufe.QueryResponse;
import com.kii.platform.ufp.ufe.client.http.stateless.ACLClient;
import com.kii.platform.ufp.ufe.client.http.stateless.BucketClient;
import com.kii.platform.ufp.ufe.errors.BucketNotFoundException;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import org.apache.commons.lang3.StringUtils;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;

/**
 * Created by ethan on 14-8-1.
 */

@Component
public class CommBucketOperate {


	private BucketClient getBucketClient(){
		return context.getBucketClient();
	};

	private ACLClient getACLClient(){
		return context.getACLClient();
	}

	@Autowired
	private AppContext context;




	public JSONObject getObjByID(String bucketName, String id) {

		try {

			TokenInfo token=context.getTokenInfo();

			return getBucketClient().getDataObject(token.getAccessToken(), context.getCurrScope(), new BucketID(bucketName), BucketType.DEFAULT, new ObjectID(id));
		} catch (KiiException e) {
			throw new KiiRuntimeException(e);
		}

	}

	public void updateParticObjWithVer(String bucketName, String id, String version, Map<String, Object> newValues) {

		try {

			JSONObject newObj = new JSONObject();
			for (Map.Entry<String, Object> entry : newValues.entrySet()) {
				newObj.put(entry.getKey(), entry.getValue());
			}

			EntityTagID entityTagID = new EntityTagID(version);
			TokenInfo token=context.getTokenInfo();
			getBucketClient().updateDataObjectPartial(token.getAccessToken(), context.getCurrScope(),
					new BucketID(bucketName), BucketType.DEFAULT, new ObjectID(id), entityTagID, newObj);

		} catch (KiiException e) {
			throw new KiiRuntimeException(e);
		} catch (JSONException ex) {
			throw new IllegalArgumentException(ex);
		}

	}

	public void updateParticObj(String bucketName, String id, Map<String, Object> newValues) {

		try {

			JSONObject newObj = new JSONObject();
			for (Map.Entry<String, Object> entry : newValues.entrySet()) {
				newObj.put(entry.getKey(), entry.getValue());
			}
			TokenInfo token=context.getTokenInfo();
			getBucketClient().updateDataObjectPartial(token.getAccessToken(), context.getCurrScope(), new BucketID(bucketName), BucketType.DEFAULT, new ObjectID(id), newObj);

		} catch (KiiException e) {
			throw new KiiRuntimeException(e);
		} catch (JSONException ex) {
			throw new IllegalArgumentException(ex);
		}

	}

	public ObjectID addObject(String bucketName, JSONObject newObj) {
		try {

			TokenInfo token=context.getTokenInfo();
			ObjectCreationResponse response= getBucketClient().createDataObject(
					token.getAccessToken(),
					context.getCurrScope(),
					new BucketID(bucketName),
					BucketType.DEFAULT, DataType.APPLICATION_JSON, newObj);


			return response.getObjectID();

		} catch (KiiException e) {
			throw new KiiRuntimeException(e);
		}
	}



	public List<JSONObject>  queryAll(String bucketID){

		List<JSONObject>  list=new ArrayList<JSONObject>();

		BucketQuery query=new BucketQuery( BucketQuery.qAll());

		QueryResult result=query(bucketID,query,null);
		list.addAll(result.getResultList());

		if(StringUtils.isNoneBlank(result.getPageToken())){

			result=query(bucketID,query,result.getPageToken());
			list.addAll(result.getResultList());
		}

		return list;

	}

	public QueryResult query(String bucketID,BucketQuery query){
		return query(bucketID,query,null);

	}


	public QueryResult query(String bucketID,BucketQuery query,String  pageToken){

		QueryRequest queryRequest = new QueryRequest(query);
		if(StringUtils.isNotBlank(pageToken)) {
			queryRequest.setPaginationKey(new PaginationKey(pageToken));
		}
		try {
			TokenInfo token=context.getTokenInfo();
			QueryResponse queryResponse = getBucketClient().query(token.getAccessToken(),
					context.getCurrScope(), new BucketID(bucketID),
					BucketType.DEFAULT, queryRequest);

			return new QueryResult(queryResponse);

		} catch (BucketNotFoundException e) {
			throw new IllegalArgumentException(e);
		} catch (JSONException ex) {
			throw new IllegalArgumentException(ex);

		}

	}

	public static class QueryResult{

		public QueryResult(QueryResponse queryResponse) throws JSONException {
			JSONArray array=queryResponse.getResults();

			for(int i=0;i<array.length();i++){
				resultList.add(array.getJSONObject(i));
			}

			if(queryResponse.getNextPaginationKey()!=null) {
				pageToken = queryResponse.getNextPaginationKey().toString();
			}

		}

		private List<JSONObject> resultList=new ArrayList<JSONObject>();

		private String pageToken;

		public String getPageToken(){
			return pageToken;
		}

		public List<JSONObject> getResultList(){
			return resultList;
		}

	}

}
