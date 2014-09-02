package com.kii.app.youwill.iap.server.dao;

import com.kii.platform.ufp.ufe.QueryResponse;
import org.codehaus.jettison.json.JSONArray;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.util.ArrayList;
import java.util.List;

/**
 * Created by ethan on 14-8-19.
 */
public  class QueryResult{

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

