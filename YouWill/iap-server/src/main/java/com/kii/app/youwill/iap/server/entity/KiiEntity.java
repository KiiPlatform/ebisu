package com.kii.app.youwill.iap.server.entity;

import com.kii.app.youwill.iap.server.service.IAPErrorCode;
import com.kii.app.youwill.iap.server.service.ServiceException;
import org.codehaus.jettison.json.JSONException;
import org.codehaus.jettison.json.JSONObject;

import java.util.Date;

/**
 * Created by ethan on 14-8-1.
 */
public class KiiEntity {
    /*
	_id ... The object's internal identifier assigned by Kii Cloud or explicitly by you.
_version ... The version of the object.
_dataType ... The data type of the object.
_owner ... The user ID of the object owner.
_created ... The object's created date in UNIX time (msec)
_modified ... The object's last updated date in UNIX time (msec)

	 */

    public KiiEntity() {

    }

    private JSONObject jsonObject;

    public KiiEntity(JSONObject json) {
        try {
            jsonObject = json;
            id = json.getString("_id");
            version = json.getString("_version");
            ownerID = json.getString("_owner");
            created = new Date(json.getLong("_created"));
            modified = new Date(json.getLong("_modified"));

        } catch (JSONException e) {

            throw new ServiceException(IAPErrorCode.FORMAT_INVALID);
        }

    }

    private String id;

    private String version;

    private String ownerID;

    private Date created;

    private Date modified;

    public String getId() {
        return id;
    }

    public void setId(String id) {
        this.id = id;
    }

    public String getVersion() {
        return version;
    }

    public void setVersion(String version) {
        this.version = version;
    }

    public String getOwnerID() {
        return ownerID;
    }

    public void setOwnerID(String ownerID) {
        this.ownerID = ownerID;
    }

    public Date getCreated() {
        return created;
    }

    public void setCreated(Date created) {
        this.created = created;
    }

    public Date getModified() {
        return modified;
    }

    public void setModified(Date modified) {
        this.modified = modified;
    }

    public String getFieldByName(String name) {
        if (jsonObject == null) {
            return null;
        }
        return jsonObject.optString(name, null);
    }


}
