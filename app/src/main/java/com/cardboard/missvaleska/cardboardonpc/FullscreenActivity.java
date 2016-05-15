package com.cardboard.missvaleska.cardboardonpc;

import com.cardboard.missvaleska.cardboardonpc.util.SystemUiHider;
import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

import android.annotation.TargetApi;
import android.app.Activity;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.MotionEvent;
import android.view.View;

import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.widget.TextView;
import android.widget.Toast;
import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.PackageManager;

import javax.microedition.khronos.egl.EGLConfig;


/**
 * An example full-screen activity that shows and hides the system UI (i.e.
 * status bar and navigation/system bar) with user interaction.
 *
 * @see SystemUiHider
 */

public class FullscreenActivity extends CardboardActivity implements /*SensorEventListener,TODO:delete sensor stuff*/ CardboardView.Renderer {

    private TextView tv;
    /*TODO:delete sensor stuff
    private SensorManager mSensorManager;
    private Sensor mGyroSensor;
    */
    private HeadTransform headTransform;

    /**
     * Whether or not the system UI should be auto-hidden after
     * {@link #AUTO_HIDE_DELAY_MILLIS} milliseconds.
     */
    private static final boolean AUTO_HIDE = true;

    /**
     * If {@link #AUTO_HIDE} is set, the number of milliseconds to wait after
     * user interaction before hiding the system UI.
     */
    private static final int AUTO_HIDE_DELAY_MILLIS = 3000;

    /**
     * If set, will toggle the system UI visibility upon interaction. Otherwise,
     * will show the system UI visibility upon interaction.
     */
    private static final boolean TOGGLE_ON_CLICK = true;

    /**
     * The flags to pass to {@link SystemUiHider#getInstance}.
     */
    private static final int HIDER_FLAGS = SystemUiHider.FLAG_HIDE_NAVIGATION;

    /**
     * The instance of the {@link SystemUiHider} for this activity.
     */
    private SystemUiHider mSystemUiHider;

    public native void QuaternionToPC(float[] headrot);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_fullscreen);

        CardboardView cardboardView = (CardboardView) findViewById(R.id.cardboard_view);
        cardboardView.setRenderer(this);
        cardboardView.setTransitionViewEnabled(false);
        setCardboardView(cardboardView);


        final View controlsView = findViewById(R.id.fullscreen_content_controls);
        final View contentView = findViewById(R.id.fullscreen_content);

        // Set up an instance of SystemUiHider to control the system UI for
        // this activity.
        mSystemUiHider = SystemUiHider.getInstance(this, contentView, HIDER_FLAGS);
        mSystemUiHider.setup();
        mSystemUiHider
                .setOnVisibilityChangeListener(new SystemUiHider.OnVisibilityChangeListener() {
                    // Cached values.
                    int mControlsHeight;
                    int mShortAnimTime;

                    @Override
                    @TargetApi(Build.VERSION_CODES.HONEYCOMB_MR2)
                    public void onVisibilityChange(boolean visible) {
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.HONEYCOMB_MR2) {
                            // If the ViewPropertyAnimator API is available
                            // (Honeycomb MR2 and later), use it to animate the
                            // in-layout UI controls at the bottom of the
                            // screen.
                            if (mControlsHeight == 0) {
                                mControlsHeight = controlsView.getHeight();
                            }
                            if (mShortAnimTime == 0) {
                                mShortAnimTime = getResources().getInteger(
                                        android.R.integer.config_shortAnimTime);
                            }
                            controlsView.animate()
                                    .translationY(visible ? 0 : mControlsHeight)
                                    .setDuration(mShortAnimTime);
                        } else {
                            // If the ViewPropertyAnimator APIs aren't
                            // available, simply show or hide the in-layout UI
                            // controls.
                            controlsView.setVisibility(visible ? View.VISIBLE : View.GONE);
                        }

                        if (visible && AUTO_HIDE) {
                            // Schedule a hide().
                            delayedHide(AUTO_HIDE_DELAY_MILLIS);
                        }
                    }
                });

        // Set up the user interaction to manually show or hide the system UI.
        contentView.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                if (TOGGLE_ON_CLICK) {
                    mSystemUiHider.toggle();
                } else {
                    mSystemUiHider.show();
                }
            }
        });

        // Upon interacting with UI controls, delay any scheduled hide()
        // operations to prevent the jarring behavior of controls going away
        // while interacting with the UI.
        findViewById(R.id.dummy_button).setOnTouchListener(mDelayHideTouchListener);

        /* TODO:delete sensor stuff
        //gyro stuff!
        tv= (TextView)findViewById(R.id.txt2);
        // Get an instance of the sensor service
        mSensorManager = (SensorManager) getSystemService(Context.SENSOR_SERVICE);
        mGyroSensor=mSensorManager.getDefaultSensor(Sensor.TYPE_ROTATION_VECTOR);

        PackageManager PM= this.getPackageManager();
        boolean gyro = PM.hasSystemFeature(PackageManager.FEATURE_SENSOR_GYROSCOPE);
        boolean light = PM.hasSystemFeature(PackageManager.FEATURE_SENSOR_LIGHT);

        if(gyro){

            if(light){
                Toast.makeText(getApplicationContext(),"Both light and gyroscope sensors are present", Toast.LENGTH_LONG).show();
            }
            else
                Toast.makeText(getApplicationContext(),"Only gyroscope sensor is present", Toast.LENGTH_LONG).show();

        }
        */

        headTransform = new HeadTransform();

        System.loadLibrary("CardboardToPC");

    }

    /*TODO:delete sensor stuff
    @Override
    public final void onAccuracyChanged(Sensor sensor, int accuracy) {
        // Do something if sensor accuracy changes.
    }
    */

    /** The points. */
    protected float points[] = { 0, 0, 0, 0 };

    /**
     * Instantiates a new vector4f.
     *
     * @param x the x
     * @param y the y
     * @param z the z
     * @param w the w
     */

    public void setXYZW(float x, float y, float z, float w) {
        this.points[0] = x;
        this.points[1] = y;
        this.points[2] = z;
        this.points[3] = w;
    }

    //just to satisfy the interface
    @Override
    public void onDrawFrame(HeadTransform headTransform, Eye eye, Eye eye1) {
        float[] eulerAngles = new float[4];//TODO: change back to 3 when not sending through the quaterion interface
        headTransform.getEulerAngles(eulerAngles, 0);//angles are in radians

        //send through the quaternion interface for now
        QuaternionToPC(eulerAngles);
    }

    @Override
    public void onFinishFrame(Viewport viewport) {
        //for now, we do nothing
    }

    @Override
    public void onSurfaceChanged(int i, int i1) {
        //for now, we do nothing
    }

    @Override
    public void onSurfaceCreated(EGLConfig eglConfig) {
        //for now, we do nothing
    }

    @Override
    public void onRendererShutdown() {
        //for now, we do nothing
    }


    /*TODO:delete sensor stuff
    @Override
    public final void onSensorChanged(SensorEvent event) {


        // we received a sensor event. it is a good practice to check
        // that we received the proper event
        if (event.sensor.getType() == Sensor.TYPE_ROTATION_VECTOR) {
            // convert the rotation-vector to a 4x4 matrix. the matrix
            // is interpreted by Open GL as the inverse of the
            // rotation-vector, which is what we want.
            float[] deltaRotationMatrix = new float[9];
            SensorManager.getRotationMatrixFromVector(deltaRotationMatrix, event.values);

            // Get Quaternion
            float[] q = new float[4];
            // Calculate angle. Starting with API_18, Android will provide this value as event.values[3], but if not, we have to calculate it manually.
            SensorManager.getQuaternionFromVector(q, event.values);
            setXYZW(event.values[0], event.values[2], event.values[3], -1);

            //Log.d("Rot", java.util.Arrays.toString(q));
            //Log.d("Property", System.getProperty("java.library.path"));

            //QuaternionToPC(points); TODO: uncomment back in
            /*float angularXSpeed = event.values[0];
            tv.setText("Angular X speed level is: " + "" + angularXSpeed);*//*
        }

    }
    */

    @Override
    protected void onResume() {
        // Register a listener for the sensor.
        super.onResume();
        //TODO:delete sensor stuff
        //mSensorManager.registerListener(this, mGyroSensor, SensorManager.SENSOR_DELAY_FASTEST);
    }

    @Override
    protected void onPause() {
        // important to unregister the sensor when the activity pauses.
        super.onPause();
        //TODO:delete sensor stuff
        //SensorManager.unregisterListener(this);
    }

    @Override
    protected void onPostCreate(Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);

        // Trigger the initial hide() shortly after the activity has been
        // created, to briefly hint to the user that UI controls
        // are available.
        delayedHide(100);
    }


    /**
     * Touch listener to use for in-layout UI controls to delay hiding the
     * system UI. This is to prevent the jarring behavior of controls going away
     * while interacting with activity UI.
     */
    View.OnTouchListener mDelayHideTouchListener = new View.OnTouchListener() {
        @Override
        public boolean onTouch(View view, MotionEvent motionEvent) {
            if (AUTO_HIDE) {
                delayedHide(AUTO_HIDE_DELAY_MILLIS);
            }
            return false;
        }
    };

    Handler mHideHandler = new Handler();
    Runnable mHideRunnable = new Runnable() {
        @Override
        public void run() {
            mSystemUiHider.hide();
        }
    };

    /**
     * Schedules a call to hide() in [delay] milliseconds, canceling any
     * previously scheduled calls.
     */
    private void delayedHide(int delayMillis) {
        mHideHandler.removeCallbacks(mHideRunnable);
        mHideHandler.postDelayed(mHideRunnable, delayMillis);
    }
}
