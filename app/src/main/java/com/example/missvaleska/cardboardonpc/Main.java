package com.example.missvaleska.cardboardonpc;

/**
 * Created by missvaleska on 3/20/15.
 */

import android.content.Context;
import android.opengl.Matrix;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.Log;

import com.badlogic.gdx.math.Matrix4;
import com.badlogic.gdx.math.Quaternion;
import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;
import com.google.vrtoolkit.cardboard.sensors.HeadTracker;

import java.util.Arrays;

import javax.microedition.khronos.egl.EGLConfig;

import static java.security.AccessController.getContext;

public class Main extends CardboardActivity implements CardboardView.StereoRenderer {

    private float[] headpos;
    private float[] headMatrix;
    private Vibrator vibrator;
    private float[] modelCube;
    private float[] camera;
    private static final float CAMERA_Z = 0.01f;
    private static final float TIME_DELTA = 0.3f;

    Matrix4 tmpMat = new Matrix4();
    Quaternion tmpQuat = new Quaternion();

    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_fullscreen);
        CardboardView cardboardView = (CardboardView) findViewById(R.id.fullscreen_content);
        cardboardView.setRenderer(this);
        setCardboardView(cardboardView);

        headpos =  new float[4];
        headMatrix = new float[16];
        vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
    }
/**
* Prepares OpenGL ES before we draw a frame.
*@param headTransform The head transformation in the new frame.
 */
    public void onNewFrame(HeadTransform headTransform) {
        // Build the Model part of the ModelView matrix.
        Matrix.rotateM(modelCube, 0, TIME_DELTA, 0.5f, 0.5f, 1.0f);

        // Build the camera matrix and apply it to the ModelView.
        Matrix.setLookAtM(camera, 0, 0.0f, 0.0f, CAMERA_Z, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

        headTransform.getQuaternion(headpos, 0);
        for (int i = 0; i < 10; i++) {
            Log.d("HeadTransform Value:", headTransform.toString());
        }

    }
/**
 * Draws a frame for an eye.
 *
 * @param eye The eye to render. Includes all required transformations.
 */
@Override
public void onDrawEye(Eye eye) {

    }
@Override
public void onRendererShutdown() {
    Log.i("Shutdown", "onRendererShutdown");
    }
@Override
public void onFinishFrame(Viewport viewport) {

    }
/**
* Creates the buffers we use to store information about the 3D world.
*
* <p>OpenGL doesn't use Java arrays, but rather needs data in a format it can understand.
* Hence we use ByteBuffers.
*
* @param config The EGL configuration used when creating the surface.
*/
@Override
public void onSurfaceCreated(EGLConfig config) {
    Log.i("Created", "onSurfaceCreated");
    }
@Override
public void onSurfaceChanged(int width, int height) {
    Log.i("Changed", "onSurfaceChanged");
    }
/**
* Called when the Cardboard trigger is pulled.
*/
@Override
public void onCardboardTrigger() {
    Log.i("Trigger", "onCardboardTrigger");

    HeadTracker headTracker = HeadTracker.createFromContext(getContext());

    headTracker.getLastHeadView(headMatrix, 0);

    headTracker.startTracking();

     tmpMat.set(headMatrix);
     tmpQuat.setFromMatrix(tmpMat);
     tmpQuat.x *= -1;
     tmpQuat.y *= -1;
     tmpQuat.z *= -1;

    Log.d("Rotation Data", java.util.Arrays.toString(tmpQuat.toString()));

        // Always give user feedback.
        vibrator.vibrate(50);
    }
}
