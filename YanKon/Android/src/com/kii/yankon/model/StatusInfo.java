package com.kii.yankon.model;

import com.kii.yankon.utils.Constants;

/**
 * Created by Evan on 15/1/15.
 */
public class StatusInfo {
    public String id;

    public boolean state = true;
    public int color = Constants.DEFAULT_COLOR;
    public int brightness = Constants.DEFAULT_BRIGHTNESS;
    public int CT = Constants.DEFAULT_CT;

    @Override
    public boolean equals(Object o) {
        if (o == null || !(o instanceof StatusInfo)) {
            return false;
        }
        StatusInfo other = (StatusInfo) o;
        return other.id.equals(id);
    }
}
