package com.kii.app.youwill.iap.server.dao.impl;


import com.kii.app.youwill.iap.server.dao.CommBucketOperate;
import com.kii.app.youwill.iap.server.dao.ProductDao;
import com.kii.app.youwill.iap.server.entity.Product;
import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import com.kii.app.youwill.iap.server.web.AppContext;
import org.codehaus.jettison.json.JSONObject;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.stereotype.Component;

/**
 * Created by ethan on 14-7-25.
 */
@Component
public class ProductDaoImpl implements ProductDao {

	@Autowired
	private CommBucketOperate commDao;
	@Autowired
	private AppContext context;


	private static final String BUCKET_ID="product";

	@Override
	public Product getProductByID(String productID) {
        JSONObject obj = null;
        try {
            obj = commDao.getObjByID(BUCKET_ID, productID);
        } catch (Exception e) {
            e.printStackTrace();
        }
		if(obj == null){
			throw new ServiceException(IAPErrorCode.PRODUCT_NOT_FOUND);
		}
		return new Product(obj);

	}
}
