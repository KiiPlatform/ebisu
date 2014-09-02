package com.kii.app.youwill.iap.server.dao.impl;


import com.kii.app.youwill.iap.server.aop.KiiScope;
import com.kii.app.youwill.iap.server.aop.ScopeType;
import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.dao.ProductDao;
import com.kii.app.youwill.iap.server.entity.Product;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.app.youwill.iap.server.web.AppContext;
import com.kii.platform.ufp.ufe.query.BucketQuery;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

/**
 * Created by ethan on 14-7-25.
 */
@Component
@KiiScope(scope= ScopeType.Admin)
public class ProductDaoImpl implements ProductDao {

	@Autowired
	private CommBucketOperate commDao;
	@Autowired
	private AppContext context;


	private static final String BUCKET_ID="product";

	@Override
	public Product getProductByID(String productID) {


		BucketQuery query=new BucketQuery(BucketQuery.qEquals("productID", productID));

		CommBucketOperate.QueryResult result=commDao.query(BUCKET_ID,query);

		if(result.getResultList().size()!=1){
			throw new ServiceException(IAPErrorCode.PRODUCT_NOT_FOUND);
		}
		return new Product(result.getResultList().get(0));

	}
}
