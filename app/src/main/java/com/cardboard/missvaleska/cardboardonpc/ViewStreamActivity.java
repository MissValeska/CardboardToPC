package com.cardboard.missvaleska.cardboardonpc;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.media.MediaPlayer;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.EditText;
import android.widget.Toast;

import com.cardboard.missvaleska.cardboardonpc.util.SystemUiHider;
import com.google.vrtoolkit.cardboard.CardboardActivity;
import com.google.vrtoolkit.cardboard.CardboardView;
import com.google.vrtoolkit.cardboard.Eye;
import com.google.vrtoolkit.cardboard.HeadTransform;
import com.google.vrtoolkit.cardboard.Viewport;

import java.io.IOException;

import javax.microedition.khronos.egl.EGLConfig;

/**
 * Created by scott on 5/21/16.
 */
public class ViewStreamActivity extends CardboardActivity
        implements CardboardView.Renderer, MediaPlayer.OnPreparedListener, SurfaceHolder.Callback, MediaPlayer.OnErrorListener, MediaPlayer.OnCompletionListener {

    public native void QuaternionToPC(float[] headrot, String str);


    private SurfaceHolder surfaceHolder;
    private MediaPlayer mediaPlayer;

    private HeadTransform headTransform;

    private String ipAddress;
    private static final String STREAM_SUFFIX = ":8554/stream";
    private static final String STREAM_PREFIX = "rtsp://";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        System.loadLibrary("CardboardToPC");

        setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        setContentView(R.layout.activity_view_stream);

        CardboardView cardboardView = (CardboardView) findViewById(R.id.cardboardView);
        cardboardView.setRenderer(this);
        cardboardView.setTransitionViewEnabled(false);
        setCardboardView(cardboardView);

        headTransform = new HeadTransform();

        ipAddress = getIntent().getStringExtra("IP Address");

        SurfaceView surfaceView = (SurfaceView) findViewById(R.id.video_surface);
        surfaceHolder = surfaceView.getHolder();
        surfaceHolder.addCallback(this);
    }

    //On each cardboard frame
    @Override
    public void onDrawFrame(HeadTransform headTransform, Eye eye, Eye eye1) {

        float[] eulerAngles = new float[4];//TODO: change back to 3 when not sending through the quaterion interface
        headTransform.getEulerAngles(eulerAngles, 0);//angles are in radians

        //QuaternionToPC(eulerAngles, str);
    }

    @Override
    public void surfaceCreated(SurfaceHolder sh) {
        mediaPlayer = new MediaPlayer();
        assert(mediaPlayer != null);
        mediaPlayer.setDisplay(surfaceHolder);

        Context context = getApplicationContext();

        try {
            //Connect to the stream
            mediaPlayer.setDataSource(context, Uri.parse(STREAM_PREFIX + ipAddress + STREAM_SUFFIX));
        }
        catch (IOException e){
            Log.e("CardboardToPC", e.getMessage());

            fatalError("IOException while setting data source.");
        }

        //settings
        mediaPlayer.setOnPreparedListener(this);
        mediaPlayer.setScreenOnWhilePlaying(true);
        mediaPlayer.setOnErrorListener(this);
        mediaPlayer.setOnCompletionListener(this);

        //start loading asymmetrically
        mediaPlayer.prepareAsync();
    }

    private void fatalError(String errorMessage){
        Toast.makeText(getApplicationContext(), errorMessage, Toast.LENGTH_LONG).show();
        finish();
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder sh) {
        mediaPlayer.release();
    }

    //starts the player when it is ready
    @Override
    public void onPrepared(MediaPlayer mp) {
        mediaPlayer.start();
    }

    public boolean onError(MediaPlayer mediaPlayer, int what, int extra){
        if(what == MediaPlayer.MEDIA_ERROR_SERVER_DIED){
            String errorMessage = "Error of unknown type.";

            switch(extra){
                case MediaPlayer.MEDIA_ERROR_IO:
                    errorMessage = "Media IO Error";
                    break;

                case MediaPlayer.MEDIA_ERROR_MALFORMED:
                    errorMessage = "Data bitstream is malformed.";
                    break;

                case MediaPlayer.MEDIA_ERROR_UNSUPPORTED:
                    errorMessage = "Stream is not a supported type.";
                    break;

                case MediaPlayer.MEDIA_ERROR_TIMED_OUT:
                    errorMessage = "Timeout error occurred.";
                    break;
            }

            fatalError(errorMessage);

            return true;

        } else {//Unhandled exception
            return false;
        }
    }

    //if the stream stops, try to start it again
    public void onCompletion(MediaPlayer mediaPlayer){
        int completedPosition = mediaPlayer.getCurrentPosition();

        Toast.makeText(getApplicationContext(), "Completed at " + completedPosition, Toast.LENGTH_SHORT).show();
    }

    ////For now, we do nothing
    //Required by Cardboard
    @Override public void onFinishFrame(Viewport viewport) {}
    @Override public void onSurfaceChanged(int i, int i1) {}
    @Override public void onSurfaceCreated(EGLConfig eglConfig) {}
    @Override public void onRendererShutdown() {}
    //Required by surfaceView
    @Override public void surfaceChanged(SurfaceHolder sh, int f, int w, int h) {}
}
