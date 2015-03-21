package com.example.missvaleska.cardboardonpc;

/**
 * Created by missvaleska on 3/20/15.
 */

import com.google.vrtoolkit.cardboard.HeadTransform;
import java.util.Arrays;

public class ReceiveRotationData {

    private float[] headpos;

    public float[] main(HeadTransform headTransform) {

        headpos =  new float[4];
        headTransform.getQuaternion(headpos, 0);
        //overlayView.show3DToast(java.util.Arrays.toString(headpos));
        return headpos;

    }

}
